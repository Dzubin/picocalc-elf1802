/*
 * make_mcmon.c - assembles MCMON.ASM (the Membership Card's serial monitor) with
 * the project's assembler and writes mcard_monitor.h, the bytes the card holds in
 * its ROM at 8000, in the project folder.
 *
 * Build and run from the programs folder:
 *   gcc -I.. make_mcmon.c ../asm_core.c ../isa_1802.c ../disasm1802.c -o make_mcmon
 *   ./make_mcmon
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asm_core.h"
#include "isa.h"
#include "mcard_const.h"

#define SOURCE_FILE   "MCMON.ASM"
#define HEADER_FILE   "../mcard_monitor.h"
#define SOURCE_MAX    32768

extern const isa_t isa_1802;

int main(void)
{
    static char source[SOURCE_MAX];
    static asm_t assembled;
    size_t length;
    FILE *f, *h;
    int i;
    unsigned size;

    f = fopen(SOURCE_FILE, "r");
    if (!f) {
        perror(SOURCE_FILE);
        return 1;
    }
    length = fread(source, 1, sizeof source - 1, f);
    fclose(f);
    source[length] = '\0';

    if (asm_assemble(&isa_1802, &assembled, MCARD_ROM_BASE, 0x4000, source, length) != 0) {
        for (i = 0; i < assembled.error_count; i++)
            printf("MCMON line %d: %s\n", assembled.errors[i].line, assembled.errors[i].text);
        return 1;
    }
    size = (unsigned)(assembled.high - (int)MCARD_ROM_BASE + 1);
    printf("MCMON is %u bytes at %04X\n", size, (unsigned)MCARD_ROM_BASE);

    h = fopen(HEADER_FILE, "w");
    if (!h) {
        perror(HEADER_FILE);
        return 1;
    }
    fprintf(h, "/*\n * mcard_monitor.h - the serial monitor that the Membership Card holds in its\n"
               " * ROM at 8000, assembled from programs/MCMON.ASM by programs/make_mcmon.c;\n"
               " * do not edit by hand.\n */\n"
               "#ifndef MCARD_MONITOR_H\n#define MCARD_MONITOR_H\n\n#include <stdint.h>\n\n"
               "#define MCARD_MONITOR_SIZE %u\n\n"
               "static const uint8_t MCARD_MONITOR[MCARD_MONITOR_SIZE] = {", size);
    for (i = 0; i < (int)size; i++)
        fprintf(h, "%s0x%02X,", i % 12 == 0 ? "\n    " : " ", assembled.image[i]);
    fprintf(h, "\n};\n\n#endif /* MCARD_MONITOR_H */\n");
    fclose(h);
    printf("wrote %s\n", HEADER_FILE);
    return 0;
}
