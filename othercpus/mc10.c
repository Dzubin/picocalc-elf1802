/*
 * mc10.c - the TRS-80 MC-10. See mc10.h.
 *
 * The map and the port wiring follow the machine's schematic as MAME's
 * mc10.cpp has it: port 1 is the keyboard strobe (a low bit selects a row),
 * 0xBFFF reads the selected rows and writes the video chip's mode pins and the
 * sound bit, port 2 carries the cassette and one more keyboard line.
 *
 * Author: Thomas Dzubin
 */
#include <string.h>

#include "mc10.h"
#include "rom.h"

typedef struct {
    char    ch;
    uint8_t row, bit, shift;
} key_t68;

static const key_t68 key_table[] = MC10_KEYS;

/* ---------------------------------------------------------------------- */
/*  The bus                                                                */
/* ---------------------------------------------------------------------- */

static uint8_t bus_read(void *ctx, uint16_t addr)
{
    const mc10_t *m = ctx;

    return membus_read(&m->bus, addr);
}

static void bus_write(void *ctx, uint16_t addr, uint8_t data)
{
    const mc10_t *m = ctx;

    membus_write(&m->bus, addr, data);
}

/* The rows port 1 selects, ANDed: the columns of the keys that are down read low. */
static uint8_t keyboard_read(const mc10_t *m)
{
    uint8_t result = 0xFF;
    int r;

    for (r = 0; r < MC10_ROWS; r++)
        if (!(m->strobe & (1u << r)))
            result &= (uint8_t)~m->keys[r];
    return result;
}

static uint8_t io_read(void *ctx, uint16_t addr)
{
    if (addr != MC10_IO_ADDRESS)
        return MC10_OPEN_BUS;
    return keyboard_read(ctx);
}

static void io_write(void *ctx, uint16_t addr, uint8_t data)
{
    mc10_t *m = ctx;
    uint8_t mode = 0;

    if (addr != MC10_IO_ADDRESS)
        return;
    if (data & MC10_IO_AG)
        mode |= M6847_AG;
    if (data & MC10_IO_GM0)
        mode |= M6847_GM0;
    if (data & MC10_IO_GM1)
        mode |= M6847_GM1;
    if (data & MC10_IO_GM2)
        mode |= M6847_GM2 | M6847_INTEXT;
    if (data & MC10_IO_CSS)
        mode |= M6847_CSS;
    m->vdg.mode = mode;
    m->dac = (data & MC10_IO_SOUND) != 0;
}

/* The video chip reads the RAM from its start. */
static uint8_t video_read(void *ctx, uint16_t addr)
{
    const mc10_t *m = ctx;

    return addr < m->ram_size ? m->ram[addr] : MC10_OPEN_BUS;
}

/* ---------------------------------------------------------------------- */
/*  The processor's ports                                                  */
/* ---------------------------------------------------------------------- */

/* Author: Thomas Dzubin */
static uint8_t port_in(void *ctx, int port)
{
    const mc10_t *m = ctx;
    uint8_t v;

    if (port == 1)
        return m->strobe;
    if (port != 2)
        return 0xFF;
    v = MC10_P2_IDLE | MC10_P2_RS232_IN | MC10_P2_CTS;     /* the cassette bit starts low */
    if (m->cassette_in)
        v |= MC10_P2_CASSETTE_IN;
    /* control, break or shift down, on a selected row: the line goes low */
    {
        int r;

        for (r = 0; r < MC10_ROWS; r++)
            if ((MC10_KEYLINE_ROWS & (1u << r)) && !(m->strobe & (1u << r)) &&
                (m->keys[r] & MC10_KEYLINE_BIT))
                v &= (uint8_t)~MC10_P2_KEY_LINE;
    }
    return v;
}

static void port_out(void *ctx, int port, uint8_t pins, uint8_t ddr)
{
    mc10_t *m = ctx;

    (void)ddr;
    if (port == 1)
        m->strobe = pins;
    else if (port == 2)
        m->cassette_out = (pins & MC10_P2_CASSETTE_OUT) != 0;
}

/* ---------------------------------------------------------------------- */
/*  Set-up                                                                 */
/* ---------------------------------------------------------------------- */

void mc10_map_memory(mc10_t *m)
{
    membus_init(&m->bus, MC10_OPEN_BUS);
    membus_map_ram(&m->bus, MC10_RAM_BASE, m->ram_size, m->ram);
    membus_map_io(&m->bus, MC10_IO_PAGE, MEMBUS_PAGE_SIZE, io_read, io_write, m);
    membus_map_rom(&m->bus, MC10_FALLBACK_PAGE, MEMBUS_PAGE_SIZE, m->fallback);
    rom_apply(&m->bus);
}

void mc10_reset(mc10_t *m)
{
    m->strobe = 0;
    m->dac = false;
    m->cassette_out = false;
    m->vdg.mode = 0;
    cpu6803_reset(&m->cpu);
}

/* Author: Thomas Dzubin */
void mc10_init(mc10_t *m, uint32_t ram_size)
{
    static const uint8_t code[MC10_FALLBACK_SIZE] = MC10_FALLBACK_CODE;

    memset(m, 0, sizeof *m);
    m->ram_size = ram_size == MC10_RAM_20K ? MC10_RAM_20K : MC10_RAM_4K;
    memcpy(m->fallback, code, sizeof code);
    m->fallback[MC10_FALLBACK_VECTOR] = (uint8_t)(MC10_FALLBACK_PAGE >> 8);
    m->fallback[MC10_FALLBACK_VECTOR + 1] = (uint8_t)MC10_FALLBACK_PAGE;
    m->cassette_in = true;
    mc10_map_memory(m);
    m->vdg.ctx = m;
    m->vdg.read = video_read;
    mc6847_reset(&m->vdg);
    qaudio_init(&m->audio, MC10_CPU_HZ);
    m->cpu.ctx = m;
    m->cpu.mem_read = bus_read;
    m->cpu.mem_write = bus_write;
    m->cpu.port_in = port_in;
    m->cpu.port_out = port_out;
    mc10_reset(m);
}

/* ---------------------------------------------------------------------- */
/*  Running                                                                */
/* ---------------------------------------------------------------------- */

/* One instruction, with the video chip and the sound kept in step. */
static int step(mc10_t *m)
{
    bool before = m->dac;
    int n = cpu6803_step(&m->cpu);

    mc6847_advance(&m->vdg, n);
    /* a store to the sound bit happens in the last cycle of the instruction */
    qaudio_cycles(&m->audio, before, (uint32_t)(n - 1));
    qaudio_cycle(&m->audio, m->dac);
    return n;
}

uint32_t mc10_run(mc10_t *m, uint32_t budget)
{
    uint32_t done = 0;

    while (done < budget)
        done += (uint32_t)step(m);
    return done;
}

uint32_t mc10_run_to(mc10_t *m, uint32_t budget, uint16_t address, bool *hit)
{
    uint32_t done = 0;

    *hit = false;
    while (done < budget) {
        done += (uint32_t)step(m);
        if (m->cpu.pc == address) {
            *hit = true;
            break;
        }
    }
    return done;
}

uint8_t mc10_peek(const mc10_t *m, uint16_t addr)
{
    return membus_peek(&m->bus, addr);
}

/* ---------------------------------------------------------------------- */
/*  Keys and the cassette                                                  */
/* ---------------------------------------------------------------------- */

/* What the matrix shows: the keys held as themselves and as shifted symbols,
 * and SHIFT too while any shifted symbol is held. Working it out from the two
 * sets each time means that SHIFT stays down until the last key that wants it
 * is let go, and a key pressed twice (auto-repeat) is still one key. */
static void refresh_keys(mc10_t *m)
{
    bool shift = false;
    int r;

    for (r = 0; r < MC10_ROWS; r++) {
        m->keys[r] = (uint8_t)(m->plain[r] | m->symbol[r]);
        if (m->symbol[r])
            shift = true;
    }
    if (shift)
        m->keys[MC10_SHIFT_ROW] |= (uint8_t)(1u << MC10_SHIFT_BIT);
}

static void hold_key(uint8_t *set, int row, int bit, bool down)
{
    if (down)
        set[row] |= (uint8_t)(1u << bit);
    else
        set[row] &= (uint8_t)~(1u << bit);
}

void mc10_key(mc10_t *m, int row, int bit, bool down)
{
    if (row < 0 || row >= MC10_ROWS || bit < 0 || bit > 7)
        return;
    hold_key(m->plain, row, bit, down);
    refresh_keys(m);
}

bool mc10_press_char(mc10_t *m, int ch, bool down)
{
    size_t i;

    if (ch >= 'a' && ch <= 'z')
        ch = ch - 'a' + 'A';
    for (i = 0; i < sizeof key_table / sizeof key_table[0]; i++) {
        if (key_table[i].ch == ch) {
            /* a shifted symbol is its key and SHIFT, both held while it is down */
            hold_key(key_table[i].shift ? m->symbol : m->plain, key_table[i].row,
                     key_table[i].bit, down);
            refresh_keys(m);
            return true;
        }
    }
    return false;
}

void mc10_release_all(mc10_t *m)
{
    memset(m->plain, 0, sizeof m->plain);
    memset(m->symbol, 0, sizeof m->symbol);
    memset(m->keys, 0, sizeof m->keys);
}

void mc10_set_cassette_in(mc10_t *m, bool level)
{
    m->cassette_in = level;
}

/* ---------------------------------------------------------------------- */
/*  The shell's view of the machine                                        */
/* ---------------------------------------------------------------------- */

static uint32_t machine_run(void *self, uint32_t cycles)
{
    return mc10_run(self, cycles);
}

static uint32_t machine_run_to(void *self, uint32_t cycles, uint16_t address, bool *hit)
{
    return mc10_run_to(self, cycles, address, hit);
}

const machine_t mc10_machine = { "TRS-80 MC-10", MC10_CPU_HZ, machine_run, machine_run_to };
