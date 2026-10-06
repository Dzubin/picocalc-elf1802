/*
 * make_programs.c - writes the demo programs, as raw 4K memory images that the
 * Elf1802 "Open" menu loads from /ELF1802/ on the SD card:
 *
 *   PIXIE.BIN   a test picture on the 1861: a frame and two diagonals, 64 x 128
 *   QTONE.BIN   the Q line toggled at about 440 Hz (the Q LED, and a tone)
 *   RANDTONE.BIN  a random pitch on the Q line, held for a while, over and over,
 *               with its number on the two hex displays: three loops, written
 *               as assembly source in this file. It is also the program the Elf
 *               holds in memory at power-on, so this tool writes boot_program.h
 *               and boot_source.h (in the folder above) and RANDTONE.ASM as
 *               well; run this tool again whenever the source changes.
 *
 * Build and run it from this folder (plain C, no hardware needed; it uses the
 * project's assembler):
 *
 *     gcc -I.. make_programs.c ../asm_core.c ../isa_1802.c ../disasm1802.c -o make_programs
 *     ./make_programs
 *
 * PIXIE.BIN uses the standard way to run the 1861 on an Elf: an interrupt
 * routine at R1 that sets R0 to the picture and uses exactly the 29 cycles
 * between the interrupt cycle and the first DMA burst (a NOP, then two-cycle
 * instructions), and a main program (at R3, so that R0 is free for the DMA)
 * made only of two-cycle instructions.
 *
 * Author: Thomas Dzubin
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "asm_core.h"
#include "elf_const.h"
#include "isa.h"

#define IMAGE_SIZE      4096
#define ISR_ENTRY       0x0020          /* R1 points here; the RET is just before */
#define MAIN_AT         0x0017
#define PICTURE_AT      0x0400
#define PICTURE_LINES   128
#define PICTURE_BYTES   8               /* bytes per line (64 pixels)       */

#define BOOT_IMAGE_SIZE 0x0100          /* the part of the image that is kept */
#define BOOT_HEADER     "../boot_program.h"
#define SOURCE_FILE     "RANDTONE.ASM"
#define SOURCE_HEADER   "../boot_source.h"
#define SOURCE_MAX      8192            /* room for the source text         */

static int make_pixie(void);
static int make_qtone(void);
static int make_randtone(void);

int main(void)
{
    return make_pixie() | make_qtone() | make_randtone();
}

static void put(uint8_t *image, int at, const uint8_t *bytes, int n)
{
    memcpy(image + at, bytes, (size_t)n);
}

static void set_pixel(uint8_t *picture, int x, int y)
{
    picture[y * PICTURE_BYTES + x / 8] |= (uint8_t)(0x80 >> (x % 8));
}

static int write_image(const char *name, const uint8_t *image)
{
    FILE *f = fopen(name, "wb");

    if (!f) {
        perror(name);
        return 1;
    }
    fwrite(image, 1, IMAGE_SIZE, f);
    fclose(f);
    printf("wrote %s\n", name);
    return 0;
}

/* Author: Thomas Dzubin */
static int make_pixie(void)
{
    static uint8_t image[IMAGE_SIZE];
    static const uint8_t boot[] = {
        0xF8, 0x00, 0xB1, 0xF8, ISR_ENTRY, 0xA1,     /* R1 = the interrupt routine */
        0xF8, 0x00, 0xB2, 0xF8, 0xFF, 0xA2,          /* R2 = the stack            */
        0xF8, 0x00, 0xB3, 0xF8, MAIN_AT, 0xA3,       /* R3 = the main program     */
        0xE2,                                        /* X = 2                     */
        0x69,                                        /* INP 1: display on         */
        0xD3                                         /* P = 3: R0 is for the DMA  */
    };
    static const uint8_t main_loop[] = {
        0x30, MAIN_AT                                /* BR *, two cycles          */
    };
    static const uint8_t isr[] = {
        0x70,                                        /* the RET of the last interrupt */
        0x22, 0x78,                                  /* DEC 2, SAV: save T        */
        0xC4,                                        /* NOP (3 cycles)            */
        0xF8, PICTURE_AT >> 8, 0xB0,                 /* R0 = the picture          */
        0xF8, PICTURE_AT & 0xFF, 0xA0,
        0xE2, 0xE2, 0xE2, 0xE2, 0xE2, 0xE2, 0xE2,    /* 7 two-cycle fillers       */
        0x30, ISR_ENTRY - 1                          /* BR to the RET             */
    };
    uint8_t *picture = image + PICTURE_AT;
    int x, y;

    put(image, 0, boot, (int)sizeof boot);
    put(image, MAIN_AT, main_loop, (int)sizeof main_loop);
    put(image, ISR_ENTRY - 1, isr, (int)sizeof isr);

    for (x = 0; x < 64; x++) {                       /* top and bottom edges */
        set_pixel(picture, x, 0);
        set_pixel(picture, x, PICTURE_LINES - 1);
    }
    for (y = 0; y < PICTURE_LINES; y++) {
        set_pixel(picture, 0, y);                    /* left and right edges */
        set_pixel(picture, 63, y);
        set_pixel(picture, y / 2, y);                /* the two diagonals    */
        set_pixel(picture, 63 - y / 2, y);
    }
    return write_image("PIXIE.BIN", image);
}

static int make_qtone(void)
{
    static uint8_t image[IMAGE_SIZE];
    /* half a period is about 254 machine cycles: SEQ or REQ, a load and a
     * count-down loop of 6 cycles a turn, 41 turns */
    static const uint8_t program[] = {
        0x7B,                   /* 0000 SEQ            */
        0xF8, 0x29, 0xA5,       /* 0001 LDI 41, PLO 5  */
        0x25, 0x85, 0x3A, 0x04, /* 0004 DEC 5, GLO 5, BNZ 0004 */
        0x7A,                   /* 0008 REQ            */
        0xF8, 0x29, 0xA5,       /* 0009 LDI 41, PLO 5  */
        0x25, 0x85, 0x3A, 0x0C, /* 000C DEC 5, GLO 5, BNZ 000C */
        0x30, 0x00              /* 0010 BR 0000        */
    };

    put(image, 0, program, (int)sizeof program);
    return write_image("QTONE.BIN", image);
}

/* RANDTONE, as assembly source. This text is the program: the tool assembles it
 * (with asm_core.c and isa_1802.c) into RANDTONE.BIN and boot_program.h, and also writes it out
 * as RANDTONE.ASM and boot_source.h, which is what the editor starts with. Keep
 * the lines short: the PicoCalc screen is 40 columns wide. */
static const char *const randtone_source[] = {
    "; RANDTONE: three loops make a tone,",
    "; and the 1861 shows where N is.",
    "; Outer: a new random number N, on the",
    ";   displays. Its high nibble is a row",
    ";   and its low nibble a column of a",
    ";   16 x 16 grid of 4 x 8 pixel blocks.",
    "; Middle: toggles Q till the time is up.",
    "; Inner: waits N turns between toggles.",
    "; R7 counts every inner turn, so a note",
    "; lasts the same time for any N; it",
    "; ends when R7 high byte is 0 (MIDDLE).",
    "; The picture is only 16 lines of 8",
    "; bytes at 0400; the interrupt routine",
    "; shows each line 8 times. All code",
    "; that runs with the video on uses",
    "; two-cycle instructions only. The",
    "; blocks stay lit until IN is pressed.",
    "MIDDLE  EQU  #18",
    "PICHI   EQU  04     ; picture at 0400",
    "",
    "        ORG  0000",
    "        GHI  0      ; D = 0 (page 0)",
    "        PHI  1",
    "        PHI  2",
    "        PHI  3",
    "        PHI  6",
    "        LDI  ISR",
    "        PLO  1      ; R1: interrupt",
    "        LDI  F0",
    "        PLO  2      ; R2: N, and stack",
    "        LDI  MAIN",
    "        PLO  3      ; R3: main program",
    "        LDI  SHOW",
    "        PLO  6      ; R6: subroutine",
    "        LDI  PICHI",
    "        PHI  5      ; R5: the picture",
    "        SEX  2",
    "        INP  1      ; video on",
    "        LDI  F0",
    "        STR  2      ; the first N is F0",
    "        SEP  3      ; R0 is for the DMA",
    "",
    "; Outer loop. The carries make it a",
    "; sequence of all 256 values.",
    "MAIN:",
    "NEXT:   LDN  2",
    "        SMI  00     ; DF = 1 for the ADC",
    "        ADC",
    "        ADD",
    "        ADD",
    "        ADD",
    "        STR  2",
    "        OUT  4      ; N on the displays",
    "        DEC  2",
    "        SEP  6      ; light the block",
    "        LDI  MIDDLE",
    "        PHI  7",
    "",
    "; Middle loop: wait, then toggle Q.",
    "PERIOD: LDN  2",
    "WAIT:   SMI  01     ; the inner loop",
    "        DEC  7      ; counts every turn",
    "        BNZ  WAIT",
    "        BQ   QOFF   ; Q on: go to REQ",
    "        SEQ         ; Q off: turn it on",
    "        DB   38     ; and skip the REQ",
    "QOFF:   REQ",
    "        B4   ERASE  ; IN pressed: clear",
    "AGAIN:  GHI  7",
    "        BNZ  PERIOD",
    "        BR   NEXT   ; a new note",
    "",
    "; The IN button (the I key) clears the",
    "; whole picture, 128 bytes at 0400.",
    "ERASE:  LDI  00",
    "        PLO  5      ; R5 = 0400",
    "ERASE1: STR  5",
    "        INC  5",
    "        GLO  5",
    "        SHL         ; DF = bit 7: done",
    "        LDI  00",
    "        BNF  ERASE1",
    "        BR   AGAIN",
    "",
    "; Subroutine: light the block for N.",
    "; Its byte is at 0400 + N / 2, and the",
    "; nibble depends on N bit 0.",
    "SHOW:   LDN  2",
    "        SHR         ; DF = N bit 0",
    "        PLO  5",
    "        LDI  F0     ; the left nibble",
    "        BNF  LEFT",
    "        LDI  0F     ; or the right one",
    "LEFT:   SEX  5",
    "        OR",
    "        STR  5      ; set its pixels",
    "        SEX  2",
    "        SEP  3      ; back to MAIN",
    "        BR   SHOW   ; for the next call",
    "",
    "; The interrupt routine. It saves T and",
    "; D on the stack at R2, sets R0 to the",
    "; picture exactly 29 cycles after the",
    "; interrupt, and then rewinds R0 so",
    "; each picture line is shown 8 times.",
    "; The DMA takes 8 of the 14 cycles of a",
    "; line, leaving three two-cycle",
    "; instructions: PLO 0 puts R0 back to",
    "; the line's start (D) in lines 1 to 7",
    "; of each 8, and GLO 0 saves the next",
    "; start in line 8. The BN1 is in the",
    "; 2nd slot of line 8, where EF1 is low",
    "; only at the end of the picture. The",
    "; RET is just before ISR, which R1",
    "; points to.",
    "LINES:  SEX  2      ; line 8, slot 3",
    "LINE1:  PLO  0      ; line 1: rewind",
    "        SEX  2",
    "        SEX  2",
    "        PLO  0      ; line 2",
    "        SEX  2",
    "        SEX  2",
    "        PLO  0      ; line 3",
    "        SEX  2",
    "        SEX  2",
    "        PLO  0      ; line 4",
    "        SEX  2",
    "        SEX  2",
    "        PLO  0      ; line 5",
    "        SEX  2",
    "        SEX  2",
    "        PLO  0      ; line 6",
    "        SEX  2",
    "        SEX  2",
    "        PLO  0      ; line 7",
    "        SEX  2",
    "        SEX  2",
    "        GLO  0      ; line 8: go on",
    "        BN1  LINES  ; EF1 low: the end",
    "        LDXA",
    "        RET",
    "ISR:    DEC  2",
    "        SAV",
    "        DEC  2",
    "        STR  2",
    "        NOP         ; three cycles",
    "        LDI  PICHI",
    "        PHI  0",
    "        LDI  00",
    "        PLO  0      ; R0 = 0400, D = 0",
    "        SEX  2      ; four fillers,",
    "        SEX  2      ; two cycles each",
    "        SEX  2",
    "        SEX  2",
    "        BR   LINE1  ; the DMA starts",
    NULL
};

/* Author: Thomas Dzubin */
static int make_randtone(void)
{
    static uint8_t image[IMAGE_SIZE];
    static char source[SOURCE_MAX];
    static asm_t assembled;
    size_t length = 0;
    int i, k;
    FILE *f, *h;

    for (i = 0; randtone_source[i]; i++)
        length += (size_t)sprintf(source + length, "%s\n", randtone_source[i]);
    if (asm_assemble(&isa_1802, &assembled, 0, ELF_RAM_SIZE, source, length) != 0) {
        for (i = 0; i < assembled.error_count; i++)
            printf("RANDTONE line %d: %s\n", assembled.errors[i].line, assembled.errors[i].text);
        return 1;
    }
    memcpy(image, assembled.image, IMAGE_SIZE);
    printf("RANDTONE is %u bytes\n", (unsigned)assembled.bytes);

    f = fopen(SOURCE_FILE, "w");
    h = fopen(SOURCE_HEADER, "w");
    if (!f || !h) {
        perror(f ? SOURCE_HEADER : SOURCE_FILE);
        return 1;
    }
    fputs(source, f);
    fprintf(h, "/*\n * boot_source.h - the assembly source of the boot program, shown in the\n"
               " * editor at power-on. Written by programs/make_programs.c (see\n"
               " * randtone_source there); do not edit by hand.\n */\n"
               "#ifndef BOOT_SOURCE_H\n#define BOOT_SOURCE_H\n\n"
               "#define BOOT_SOURCE_NAME \"RANDTONE\"\n\n"
               "static const char BOOT_SOURCE[] =\n");
    for (i = 0; randtone_source[i]; i++)
        fprintf(h, "    \"%s\\n\"\n", randtone_source[i]);
    fprintf(h, "    ;\n\n#endif /* BOOT_SOURCE_H */\n");
    fclose(f);
    fclose(h);
    printf("wrote %s and %s\n", SOURCE_FILE, SOURCE_HEADER);

    h = fopen(BOOT_HEADER, "w");
    if (!h) {
        perror(BOOT_HEADER);
        return 1;
    }
    fprintf(h, "/*\n * boot_program.h - the program the Elf holds in memory at power-on: a new\n"
               " * random pitch on the Q line for a while, over and over, with its number\n"
               " * on the displays. Written by programs/make_programs.c (see RANDTONE\n"
               " * there); do not edit by hand.\n */\n"
               "#ifndef BOOT_PROGRAM_H\n#define BOOT_PROGRAM_H\n\n#include <stdint.h>\n\n"
               "#define BOOT_PROGRAM_SIZE %d\n\nstatic const uint8_t BOOT_PROGRAM[BOOT_PROGRAM_SIZE] = {",
            BOOT_IMAGE_SIZE);
    for (k = 0; k < BOOT_IMAGE_SIZE; k++)
        fprintf(h, "%s0x%02X,", k % 12 == 0 ? "\n    " : " ", image[k]);
    fprintf(h, "\n};\n\n#endif /* BOOT_PROGRAM_H */\n");
    fclose(h);
    printf("wrote %s\n", BOOT_HEADER);
    return write_image("RANDTONE.BIN", image);
}
