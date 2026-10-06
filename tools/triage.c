/* triage.c - run program images in the Elf1802 emulator and report what they do. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "elf.c"        /* include the board so its static bus callbacks can be wrapped */

static elf_t m;
static unsigned io_in_ports, io_out_ports, rd_high, wr_high;
static uint8_t (*orig_in)(void *, uint8_t);
static void (*orig_out)(void *, uint8_t, uint8_t);
static uint8_t (*orig_rd)(void *, uint16_t);
static void (*orig_wr)(void *, uint16_t, uint8_t);
static uint8_t w_in(void *c, uint8_t p) { io_in_ports |= 1u << p; return orig_in(c, p); }
static void w_out(void *c, uint8_t p, uint8_t d) { io_out_ports |= 1u << p; orig_out(c, p, d); }
static uint8_t w_rd(void *c, uint16_t a) { if (a >= ELF_RAM_SIZE) rd_high++; return orig_rd(c, a); }
static void w_wr(void *c, uint16_t a, uint8_t d) { if (a >= ELF_RAM_SIZE) wr_high++; orig_wr(c, a, d); }

static int hexv(int c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; }

/* load an image: raw, or Intel HEX; returns bytes used (highest addr+1) or -1; *over = bytes above 4K
 *
 * Author: Thomas Dzubin */
static long load_file(const char *path, uint8_t *img, long *over)
{
    FILE *f = fopen(path, "rb");
    static uint8_t buf[1 << 20];
    long n, hi = 0;
    size_t len = strlen(path);
    *over = 0;
    memset(img, 0, ELF_RAM_SIZE);
    if (!f) return -1;
    n = (long)fread(buf, 1, sizeof buf, f);
    fclose(f);
    if (len > 4 && strcasecmp(path + len - 4, ".hex") == 0) {
        char *p = (char *)buf;
        buf[n] = 0;
        for (; *p; p++) {
            if (*p != ':') continue;
            int cnt = hexv(p[1]) * 16 + hexv(p[2]);
            int addr = (hexv(p[3]) << 12) | (hexv(p[4]) << 8) | (hexv(p[5]) << 4) | hexv(p[6]);
            int type = hexv(p[7]) * 16 + hexv(p[8]);
            if (type == 0)
                for (int i = 0; i < cnt; i++) {
                    int b = hexv(p[9 + 2 * i]) * 16 + hexv(p[10 + 2 * i]);
                    if (addr + i < ELF_RAM_SIZE) { img[addr + i] = (uint8_t)b; if (addr + i + 1 > hi) hi = addr + i + 1; }
                    else (*over)++;
                }
        }
        return hi;
    }
    for (long i = 0; i < n; i++) {
        if (i < ELF_RAM_SIZE) img[i] = buf[i]; else (*over)++;
    }
    return n < ELF_RAM_SIZE ? n : ELF_RAM_SIZE;
}

static void ascii_thumb(void)
{
    for (int r = 0; r < 128; r += 4) {
        for (int c = 0; c < 8; c++)
            for (int b = 7; b >= 0; b -= 2) {
                int x = ((m.video.bits[r][c] >> b) & 1) | ((m.video.bits[r][c] >> (b - 1)) & 1) |
                        ((m.video.bits[r + 2][c] >> b) & 1);
                putchar(x ? '#' : '.');
            }
        putchar('\n');
    }
}

/* Author: Thomas Dzubin */
int main(int argc, char **argv)
{
    static uint8_t img[ELF_RAM_SIZE];
    int show = argc > 2 && strcmp(argv[2], "-v") == 0;
    long over, used = load_file(argv[1], img, &over);
    uint32_t cyc, idle_cycles = 0, outside = 0, max_lit = 0, frames_changed = 0;
    static uint8_t last[128][8];
    uint32_t last_frames = 0, last_disp = 0, disp_changes = 0, q0 = 0;
    int keystep = 0;

    if (used < 0) { printf("cannot read\n"); return 1; }
    elf_init(&m);
    orig_in = m.cpu.io_in; orig_out = m.cpu.io_out; orig_rd = m.cpu.mem_read; orig_wr = m.cpu.mem_write;
    m.cpu.io_in = w_in; m.cpu.io_out = w_out; m.cpu.mem_read = w_rd; m.cpu.mem_write = w_wr;
    elf_load_image(&m, img, ELF_RAM_SIZE);
    elf_set_switches(&m, true, false, false);
    memset(last, 0, sizeof last);
    last_disp = m.display;
    (void)q0;
    for (cyc = 0; cyc < ELF_CYCLES_PER_SEC * 4; cyc++) {
        if (m.cpu.r[m.cpu.p] >= ELF_RAM_SIZE) outside++;
        if (m.cpu.phase == CPU_PHASE_IDLE) idle_cycles++;
        elf_cycle(&m);
        if (cyc % (ELF_CYCLES_PER_SEC / 10) == 0) {          /* every 100 ms tap a key */
            if ((argc > 2 && strcmp(argv[2], "-k") == 0) || show) {
                elf_key_hex(&m, (unsigned)(keystep++ % 16));
                elf_in_button(&m, true);
            }
        } else if (cyc % (ELF_CYCLES_PER_SEC / 10) == ELF_CYCLES_PER_SEC / 20) {
            elf_in_button(&m, false);
        }
        if (m.video.frames != last_frames) {
            uint32_t lit = 0;
            last_frames = m.video.frames;
            for (int r = 0; r < 128; r++) for (int c = 0; c < 8; c++) {
                lit += (uint32_t)__builtin_popcount(m.video.bits[r][c]);
            }
            if (lit > max_lit) max_lit = lit;
            if (memcmp(last, m.video.bits, sizeof last)) { frames_changed++; memcpy(last, m.video.bits, sizeof last); }
        }
        if (m.display != last_disp) { disp_changes++; last_disp = m.display; }
    }
    printf("%-34s used=%5ld over4K=%5ld video=%d lit=%4u framesChanged=%3u Qchg=%5u disp=%2u outPC=%6u idle=%6u inP=%02X outP=%02X rdHi=%u wrHi=%u\n",
           argv[1], used, over, m.video.on, max_lit, frames_changed, m.q_changes, disp_changes, outside, idle_cycles,
           io_in_ports, io_out_ports, rd_high, wr_high);
    if (show && m.video.on) ascii_thumb();
    return 0;
}
