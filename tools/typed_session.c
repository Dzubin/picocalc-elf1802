#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "elf.c"

static elf_t m;
static int logging = 0;
static void (*orig_wr)(void *, uint16_t, uint8_t);
static uint8_t (*orig_in)(void *, uint8_t);

static void w_wr(void *c, uint16_t a, uint8_t d)
{
    if (logging && a >= ELF_VDU_BASE && d != 0x20)
        printf("  VDU[%03X] <- %02X\n", a - ELF_VDU_BASE, d);
    orig_wr(c, a, d);
}

static uint8_t w_in(void *c, uint8_t p)
{
    uint8_t v = orig_in(c, p);
    if (logging && p == 7)
        printf("  INP7 -> %02X\n", v);
    return v;
}

/* Author: Thomas Dzubin */
int main(int argc, char **argv)
{
    static uint8_t img[ELF_RAM_SIZE];
    FILE *f;

    if (argc < 2 || !(f = fopen(argv[1], "rb"))) {
        fprintf(stderr, "usage: typed_session image.bin\n");
        return 1;
    }
    size_t n = fread(img, 1, sizeof img, f);
    const char *typed = argv[2];
    unsigned start = (unsigned)atoi(argv[3]);
    size_t k = 0;
    unsigned long cy;

    fclose(f);
    elf_init(&m);
    orig_wr = m.cpu.mem_write; m.cpu.mem_write = w_wr;
    orig_in = m.cpu.io_in; m.cpu.io_in = w_in;
    elf_load_image(&m, img, (uint32_t)n);
    elf_set_switches(&m, 1, 0, 0);
    logging = 1;
    for (cy = 0; cy < 223721UL * (start + 12); cy++) {
        if (cy >= 223721UL * start && cy % 22372 == 0 && typed[k] && !m.kbd_ready) {
            unsigned ch = (unsigned char)typed[k] == '|' ? 13u : (unsigned char)typed[k];
            printf("send %02X\n", ch);
            elf_ascii_key(&m, (uint8_t)ch);
            k++;
        }
        elf_cycle(&m);
    }
    return 0;
}
