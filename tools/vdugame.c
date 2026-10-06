/* vdugame.c - run a program, tap keys on the hex keypad, and print the VDU as 64 x 32 pixels. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "elf.h"

static elf_t m;

/* Author: Thomas Dzubin */
int main(int argc, char **argv)
{
    static uint8_t img[ELF_RAM_SIZE];
    FILE *f = fopen(argv[1], "rb");
    size_t n = fread(img, 1, sizeof img, f);
    unsigned secs = (unsigned)atoi(argv[2]);
    unsigned long cy;
    int r, c, text = 0;

    fclose(f);
    elf_init(&m);
    elf_load_image(&m, img, (uint32_t)n);
    elf_set_switches(&m, true, false, false);
    for (cy = 0; cy < (unsigned long)ELF_CYCLES_PER_SEC * secs; cy++) {
        if (argc > 3 && cy % (ELF_CYCLES_PER_SEC / 3) == 0) {
            elf_key_hex(&m, (unsigned)((cy / (ELF_CYCLES_PER_SEC / 3)) % 16));
            elf_in_button(&m, true);
        } else if (argc > 3 && cy % (ELF_CYCLES_PER_SEC / 3) == ELF_CYCLES_PER_SEC / 6) {
            elf_in_button(&m, false);
        }
        elf_cycle(&m);
    }
    printf("vdu writes=%u pc=%04X\n", m.vdu_writes, m.cpu.r[m.cpu.p]);
    for (r = 0; r < VDU_ROWS; r++) {
        char top[VDU_COLS * 2 + 1], bot[VDU_COLS * 2 + 1];
        for (c = 0; c < VDU_COLS; c++) {
            uint8_t v = m.vdu[r * VDU_COLS + c];
            if (v & VDU_BIT_BLOCKS) {
                top[2 * c] = (v & 8) ? '#' : '.'; top[2 * c + 1] = (v & 4) ? '#' : '.';
                bot[2 * c] = (v & 2) ? '#' : '.'; bot[2 * c + 1] = (v & 1) ? '#' : '.';
            } else {
                unsigned ch = v & VDU_CODE_MASK;
                if (ch < 32) ch += 64;
                top[2 * c] = (char)ch; top[2 * c + 1] = ' ';
                bot[2 * c] = ' '; bot[2 * c + 1] = ' ';
                text++;
            }
        }
        top[VDU_COLS * 2] = 0; bot[VDU_COLS * 2] = 0;
        printf("%s\n%s\n", top, bot);
    }
    printf("text cells=%d\n", text);
    return 0;
}
