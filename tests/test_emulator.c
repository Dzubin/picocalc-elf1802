/*
 * test_emulator.c - checks the CDP1802 core, the CDP1861 timing and the Elf
 * board on a PC. Plain ISO C, no PicoCalc parts, so it builds with any C
 * compiler from the project folder:
 *
 *     gcc -Wall -Wextra -I. tests/test_emulator.c cpu1802.c cdp1861.c elf.c \
 *         membus.c target.c rom.c hexfile.c qaudio.c disasm1802.c asm_core.c isa_1802.c source_gen.c -o test_emulator
 *
 * It prints one line per failed check and a summary, and its exit status is 0
 * when everything passed.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "cpu1802.h"
#include "cdp1861.h"
#include "elf.h"
#include "hexfile.h"
#include "disasm1802.h"
#include "qaudio.h"
#include "boot_program.h"
#include "membus.h"
#include "platform.h"
#include "rom.h"
#include "boot_source.h"
#include "asm_core.h"
#include "isa.h"
#include "source_gen.h"

#define BUS_SIZE        65536
#define STACK_AT        0x0100      /* where R2 points in most tests         */
#define IMAGE_AT        0x0400      /* the picture in the video test         */
#define ISR_AT          0x0020      /* the video interrupt routine           */
#define VIDEO_FRAMES    6           /* frames to run in the video test       */
#define BLANK_LINES     8           /* lines of EFX in a frame: 4 + 4        */

static int checks;
static int failures;

#define CHECK(cond) do { \
        checks++; \
        if (!(cond)) { \
            failures++; \
            printf("FAIL line %d: %s\n", __LINE__, #cond); \
        } \
    } while (0)

/* ------------------------------------------------------------------ */
/*  main first, then the tests it runs                                  */
/* ------------------------------------------------------------------ */
static void test_arithmetic(void);
static void test_logic_and_shifts(void);
static void test_registers_and_memory(void);
static void test_branches(void);
static void test_stack_and_io(void);
static void test_all_opcodes(void);
static void test_sequencing(void);
static void test_front_panel(void);
static void test_video(void);
static void test_ram_edge(void);
static void test_ascii_keyboard(void);
static void test_vdu(void);
static void test_hex(void);
static void test_q_period(void);
static void test_q_audio(void);
static void test_step_and_break(void);
static void test_disasm(void);
static void test_reset_turns_video_off(void);
static void test_boot_program(void);
static void test_port4_example(void);
static void test_keyboard_polls(void);
static void test_branch_targets(void);
/* The assembler. A statics-only struct is big, so one is shared. */
static asm_t assembler;

static int assemble_text(const char *text)
{
    return asm_assemble(&isa_1802, &assembler, 0, ELF_RAM_SIZE, text, strlen(text));
}

/* True when the assembler's image starts with these bytes. */
static bool image_is(const uint8_t *want, size_t n)
{
    return memcmp(assembler.image, want, n) == 0;
}

/* Author: Thomas Dzubin */
static void test_assembler(void)
{
    static const char program[] =
        "        ORG 0\n"
        "START:  LDI <TABLE      ; the low byte\n"
        "        plo R5\n"
        "        ldi >table\n"
        "        PHI 5\n"
        "LOOP:   LDA 5\n"
        "        BNZ LOOP\n"
        "        LBR START\n"
        "        SEQ\n"
        "        DB \"AB\", 0, #10, $FF\n"
        "        DW TABLE, START+1\n"
        "TABLE:  DS 2\n"
        "        END\n"
        "        this is ignored\n";
    static const uint8_t expect[] = {
        0xF8, 0x16, 0xA5, 0xF8, 0x00, 0xB5, 0x45, 0x3A, 0x06, 0xC0, 0x00, 0x00, 0x7B,
        0x41, 0x42, 0x00, 0x0A, 0xFF, 0x00, 0x16, 0x00, 0x01
    };
    static const char equs[] =
        "PC      EQU 3\n"
        "COUNT:  EQU #10\n"
        "        SEP PC\n"
        "        LDI COUNT\n"
        "        INP 7\n"
        "        OUT R4\n"
        "        LDN 1\n"
        "        B1 *\n"
        "        NOP\n"
        "        LDI 'A'\n"
        "        LDI -1\n"
        "        LDI 2FH\n"
        "        LDI 0x30\n"
        "        SKP\n"
        "        NBR\n"
        "        LBR 1234\n"
        "        LBR $12+#1\n";
    static const uint8_t equs_expect[] = {
        0xD3, 0xF8, 0x0A, 0x6F, 0x64, 0x01, 0x34, 0x06, 0xC4, 0xF8, 0x41, 0xF8, 0xFF,
        0xF8, 0x2F, 0xF8, 0x30, 0x38, 0x00, 0x38, 0x00, 0xC0, 0x12, 0x34, 0xC0, 0x00, 0x13
    };

    CHECK(assemble_text(program) == 0);
    CHECK(image_is(expect, sizeof expect));
    CHECK(assembler.bytes == sizeof expect);
    CHECK(assembler.low == 0 && assembler.high == (int)sizeof expect - 1);
    CHECK(assembler.line_count == 14);
    CHECK(assembler.lines[0].address == 0 && assembler.lines[0].count == 0);      /* ORG */
    CHECK(assembler.lines[1].address == 0 && assembler.lines[1].count == 2 &&
          assembler.lines[1].bytes[0] == 0xF8 && assembler.lines[1].bytes[1] == 0x16);
    CHECK(assembler.lines[6].address == 7 && assembler.lines[6].count == 2);      /* BNZ */
    CHECK(assembler.lines[10].count == 4);                                        /* DW */
    CHECK(assembler.symbol_count == 3);                                           /* START LOOP TABLE */

    CHECK(assemble_text(equs) == 0);
    CHECK(image_is(equs_expect, sizeof equs_expect));

    CHECK(assemble_text("") == 0 && assembler.bytes == 0 && assembler.line_count == 0);
    CHECK(assemble_text("; only a comment\n\nLABEL:\n") == 0 && assembler.bytes == 0);
    CHECK(assemble_text("ORG 0100\nNOP") == 0 && assembler.image[0x100] == 0xC4 &&
          assembler.low == 0x100);                      /* no line feed at the end */
    CHECK(assemble_text("ORG 00FE\nBR 00FF\n") == 0);   /* the operand byte is on page 01? no: 00FF is page 0 */
}

/* Author: Thomas Dzubin */
static void test_assembler_errors(void)
{
    /* each source has exactly one mistake, on line 2 */
    static const struct { const char *source; const char *message; int line; } bad[] = {
        { "NOP\nFROB 5\n",              ASM_ERR_MNEMONIC,  2 },
        { "NOP\nPLO 20\n",              ASM_ERR_REGISTER,  2 },
        { "NOP\nINP 0\n",               ASM_ERR_PORT,      2 },
        { "NOP\nINP 8\n",               ASM_ERR_PORT,      2 },
        { "NOP\nLDN 0\n",               ASM_ERR_LDN0,      2 },
        { "NOP\nLDI 100\n",             ASM_ERR_BIG,       2 },
        { "NOP\nLBR 10000\n",           ASM_ERR_BIG,       2 },
        { "NOP\nBR 0200\n",             ASM_ERR_PAGE,      2 },
        { "NOP\nLBR NOWHERE\n",         ASM_ERR_UNDEFINED, 2 },
        { "X: NOP\nX: NOP\n",           ASM_ERR_DUPLICATE, 2 },
        { "NOP\nADD: NOP\n",            ASM_ERR_LABEL_NUM, 2 },
        { "NOP\nR5: NOP\n",             ASM_ERR_LABEL_NUM, 2 },
        { "NOP\nLDI\n",                 ASM_ERR_MISSING,   2 },
        { "NOP\nNOP 5\n",               ASM_ERR_EXTRA,     2 },
        { "NOP\nDB \"abc\n",            ASM_ERR_STRING,    2 },
        { "NOP\nLDI 1 2\n",             ASM_ERR_EXTRA,     2 },
        { "NOP\nORG FWD\nFWD: NOP\n",   ASM_ERR_KNOWN,     2 },
        { "NOP\nORG 3FFF\nLBR 0\n",     ASM_ERR_RAM,       3 },
        { "ORG 10\nNOP\nORG 10\nNOP\n", ASM_ERR_OVERLAP,   4 },
        { "NOP\n5 NOP\n",               ASM_ERR_MNEMONIC,  2 },
        { "NOP\nLDI 2G\n",              ASM_ERR_NUMBER,    2 },
    };
    size_t i;

    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        int errors = assemble_text(bad[i].source);

        CHECK(errors >= 1);
        if (errors < 1)
            continue;
        /* the first error is on the line the source meant, with the message */
        CHECK(assembler.errors[0].line == bad[i].line);
        CHECK(strcmp(assembler.errors[0].text, bad[i].message) == 0);
    }
    CHECK(assemble_text("BR 0200\nNOP\n") == 1 && (assembler.lines[0].flags & ASM_LINE_BAD) != 0 &&
          (assembler.lines[1].flags & ASM_LINE_BAD) == 0);                 /* one bad line is one error */
    {
        char text[600];                                 /* many mistakes: all are counted */
        int n;

        text[0] = '\0';
        for (n = 0; n < 40; n++)
            strcat(text, "FROB\n");
        CHECK(assemble_text(text) == 40);
        CHECK(assembler.errors[ASM_MAX_ERRORS - 1].line == ASM_MAX_ERRORS);
    }
}

/* The disassembler's text, typed back in, gives the same bytes, and source made
 * from an image assembles back to the image.
 *
 * Author: Thomas Dzubin */
static void test_source_round_trip(void)
{
    static uint8_t image[ELF_RAM_SIZE];
    static char text[40000];
    uint32_t done = 0;
    unsigned op, seed = 12345, i;
    size_t n;
    int pass;

    for (op = 0; op < 256; op++) {              /* every instruction by itself */
        uint8_t b[3] = { (uint8_t)op, 0x34, 0x56 };
        char line[48], source[64];
        int len;

        if (op == 0x68)
            continue;                           /* not an 1802 instruction */
        len = dis1802(0, b, line, sizeof line);
        snprintf(source, sizeof source, "%s\n", line);
        CHECK(assemble_text(source) == 0);
        if (op == 0x38)                         /* SKP: the byte it skips is not in the text */
            len = 1;
        CHECK(memcmp(assembler.image, b, (size_t)len) == 0);
    }

    /* the power-on program (its table is data, which comes back as DB or code
     * that gives the same bytes) and bytes that are not code at all */
    for (pass = 0; pass < 2; pass++) {
        memset(image, 0, sizeof image);
        if (pass == 0) {
            memcpy(image, BOOT_PROGRAM, BOOT_PROGRAM_SIZE);
        } else {
            for (i = 0; i < 600; i++) {
                seed = seed * 1103515245u + 12345u;
                image[i] = (uint8_t)(seed >> 16);
            }
        }
        n = srcgen_from_image(&isa_1802, image, sizeof image, text, sizeof text, &done);
        CHECK(n > 0 && n < sizeof text - 1);
        CHECK(assemble_text(text) == 0);
        CHECK(memcmp(assembler.image, image, sizeof image) == 0);
    }
    CHECK(strncmp(text, "ORG 0000\n", 9) == 0);

    /* a small program: the branch targets get labels */
    memset(image, 0, sizeof image);
    image[0] = 0xF8; image[1] = 0x05;           /* 0000 LDI 05    */
    image[2] = 0xFF; image[3] = 0x01;           /* 0002 SMI 01    */
    image[4] = 0x3A; image[5] = 0x02;           /* 0004 BNZ 0002  */
    image[6] = 0xC0; image[7] = 0x00; image[8] = 0x00;     /* 0006 LBR 0000 */
    n = srcgen_from_image(&isa_1802, image, sizeof image, text, sizeof text, &done);
    CHECK(strstr(text, "L0002:  SMI  01") != NULL);
    CHECK(strstr(text, "BNZ  L0002") != NULL);
    CHECK(strstr(text, "L0000:  LDI  05") != NULL);
    CHECK(strstr(text, "LBR  L0000") != NULL);
    CHECK(done == 7);                           /* up to the last byte that is not zero */

    /* the instructions that talk to the Elf's own parts get a note, and the
     * text with the notes still assembles to the same bytes */
    {
        static const uint8_t small[] = {
            0x6C, 0x64, 0x7B, 0x7A, 0x37, 0x02, 0x69, 0x6F, 0x61, 0x3F, 0x00, 0x36, 0x00, 0x3E, 0x00, 0x34, 0x00,
            0x3C, 0x00, 0x30, 0x00
        };
        static uint8_t small_image[64];

        memcpy(small_image, small, sizeof small);
        n = srcgen_from_image(&isa_1802, small_image, sizeof small_image, text, sizeof text, &done);
        CHECK(strstr(text, "INP  4       ; hex keypad\n") != NULL);
        CHECK(strstr(text, "OUT  4       ; hex displays\n") != NULL);
        CHECK(strstr(text, "SEQ          ; Q on\n") != NULL);
        CHECK(strstr(text, "REQ          ; Q off\n") != NULL);
        CHECK(strstr(text, "INP  1       ; 1861 video on\n") != NULL);
        CHECK(strstr(text, "OUT  1       ; 1861 video off\n") != NULL);
        CHECK(strstr(text, "INP  7       ; ASCII keyboard\n") != NULL);
        CHECK(strstr(text, "; if IN pressed\n") != NULL && strstr(text, "; if IN not pressed\n") != NULL);
        CHECK(strstr(text, "; if key waiting\n") != NULL && strstr(text, "; if no key waiting\n") != NULL);
        CHECK(strstr(text, "; if 1861 EFX low\n") != NULL && strstr(text, "; if 1861 EFX high\n") != NULL);
        CHECK(assemble_text(text) == 0);
        CHECK(memcmp(assembler.image, small_image, sizeof small_image) == 0);
    }

    /* a program that does not fit the text: what there is must still assemble
     * with no errors (no label for a target beyond the end, or in the middle of an
     * instruction) and give the same bytes as far as it goes */
    {
        static const size_t sizes[] = { 60, 333, 700, 1500, 4000 };
        static uint8_t big[4096];
        size_t z;

        for (i = 0; i < 3000; i++) {
            seed = seed * 1103515245u + 12345u;
            big[i] = (uint8_t)(seed >> 16);
        }
        for (z = 0; z < sizeof sizes / sizeof sizes[0]; z++) {
            static char cut[4001];

            n = srcgen_from_image(&isa_1802, big, sizeof big, cut, sizes[z], &done);
            CHECK(n > 0 && n < sizes[z]);
            CHECK(done > 0 || sizes[z] < 80);
            CHECK(assemble_text(cut) == 0);
            CHECK(memcmp(assembler.image, big, done) == 0);
        }
    }

    /* the listing in pieces is the same text as the listing in one go, whatever the
     * size of the pieces, and a whole program (here the power-on one) assembles from it */
    {
        static uint8_t prog[1024];
        static char whole[16384], pieces[16384];
        size_t sizes[] = { 128, 200, 1000 };
        size_t z;
        size_t whole_n;

        memset(prog, 0, sizeof prog);
        memcpy(prog, BOOT_PROGRAM, BOOT_PROGRAM_SIZE);
        whole_n = srcgen_from_image(&isa_1802, prog, sizeof prog, whole, sizeof whole, &done);
        CHECK(whole_n > 0 && done > 0x60);
        for (z = 0; z < sizeof sizes / sizeof sizes[0]; z++) {
            char piece[1000];
            size_t total = 0;
            bool finished = false;

            srcgen_listing_start(&isa_1802, prog, sizeof prog);
            while (!finished) {
                size_t got = srcgen_listing_next(piece, sizes[z], &finished);

                CHECK(got < sizes[z]);
                memcpy(pieces + total, piece, got);
                total += got;
            }
            pieces[total] = '\0';
            CHECK(total == whole_n && memcmp(pieces, whole, total) == 0);
        }
        CHECK(assemble_text(pieces) == 0);
        CHECK(memcmp(assembler.image, prog, sizeof prog) == 0);
        /* the trace found all of it to be code: only the SKP line is data */
        CHECK(strstr(whole, "DB 38 7A") != NULL && strstr(whole, "DB   ") == NULL);
        CHECK(strstr(whole, "L0072:  DEC  2") != NULL);     /* the interrupt routine, by R1 */
    }

    /* data after the code is written as compact DB, DS and string lines */
    {
        static uint8_t data[256];
        static char listing[2000];

        memset(data, 0, sizeof data);
        data[0] = 0x30; data[1] = 0x20;                     /* 0000 BR 0020 */
        for (i = 2; i < 12; i++)
            data[i] = (uint8_t)(0x80 + i);                  /* 0002 ten bytes of data */
        memcpy(data + 12, "HELLO WORLD", 11);               /* 000C a string */
        data[0x20] = 0x00;                                  /* 0020 IDL (code) */
        data[0x30] = 0x7B;                                  /* 0030 a byte after zeros */
        srcgen_from_image(&isa_1802, data, sizeof data, listing, sizeof listing, &done);
        CHECK(strstr(listing, "BR   L0020") != NULL);
        CHECK(strstr(listing, "DB   82,83,84,85,86,87,88,89\n") != NULL);
        CHECK(strstr(listing, "DB   \"HELLO WORLD\"\n") != NULL);
        CHECK(strstr(listing, "DS   ") != NULL);
        CHECK(assemble_text(listing) == 0);
        CHECK(memcmp(assembler.image, data, done) == 0);
    }

    /* a small buffer: the text stops at a whole line */
    {
        char little[40];

        n = srcgen_from_image(&isa_1802, image, sizeof image, little, sizeof little, &done);
        CHECK(n < sizeof little && little[n - 1] == '\n' && done < 7);
    }
}

static void test_video_option(void);
static void test_membus(void);
static void test_rom(void);
static void test_assembler(void);
static void test_assembler_errors(void);
static void test_source_round_trip(void);

/* Author: Thomas Dzubin */
int main(void)
{
    test_ram_edge();
    test_membus();
    test_rom();
    test_q_period();
    test_arithmetic();
    test_logic_and_shifts();
    test_registers_and_memory();
    test_branches();
    test_stack_and_io();
    test_all_opcodes();
    test_sequencing();
    test_front_panel();
    test_video();
    test_ascii_keyboard();
    test_vdu();
    test_hex();
    test_q_audio();
    test_step_and_break();
    test_disasm();
    test_reset_turns_video_off();
    test_boot_program();
    test_port4_example();
    test_keyboard_polls();
    test_branch_targets();
    test_video_option();
    test_assembler();
    test_assembler_errors();
    test_source_round_trip();

    printf("%d checks, %d failed\n", checks, failures);
    return failures != 0;
}

/* ------------------------------------------------------------------ */
/*  A bare 1802 on a 64K bus                                            */
/* ------------------------------------------------------------------ */
static uint8_t  mem[BUS_SIZE];
static cpu1802_t cpu;
static uint8_t  out_port, out_data;
static int      out_count;
static uint8_t  in_value;
static uint8_t  dma_out_data;
static int      dma_out_count;
static uint8_t  dma_in_value;

static uint8_t t_read(void *ctx, uint16_t a)               { (void)ctx; return mem[a]; }
static void    t_write(void *ctx, uint16_t a, uint8_t d)   { (void)ctx; mem[a] = d; }
static uint8_t t_in(void *ctx, uint8_t port)               { (void)ctx; (void)port; return in_value; }
static void    t_out(void *ctx, uint8_t port, uint8_t d)   { (void)ctx; out_port = port; out_data = d; out_count++; }
static uint8_t t_dma_in(void *ctx)                         { (void)ctx; return dma_in_value; }
static void    t_dma_out(void *ctx, uint8_t d)             { (void)ctx; dma_out_data = d; dma_out_count++; }

/* Memory cleared, CPU out of reset and past its initialization cycle, with
 * X = P = 0 and R(2) pointing at the stack area. */
static void setup(void)
{
    memset(mem, 0, sizeof mem);
    cpu1802_power_on(&cpu);
    cpu.ctx = NULL;
    cpu.mem_read = t_read;
    cpu.mem_write = t_write;
    cpu.io_in = t_in;
    cpu.io_out = t_out;
    cpu.dma_in = t_dma_in;
    cpu.dma_out = t_dma_out;
    cpu.mode = CPU_MODE_RESET;
    cpu1802_cycle(&cpu);
    cpu.mode = CPU_MODE_RUN;
    CHECK(cpu1802_cycle(&cpu) == CPU_CYCLE_INIT);
    cpu.r[2] = STACK_AT;
    out_count = dma_out_count = 0;
    in_value = 0;
}

static void load_program(const uint8_t *bytes, int n)
{
    memcpy(mem, bytes, (size_t)n);
}

/* Run one whole instruction; returns how many machine cycles it took. */
static int step(void)
{
    int n = 0;

    do {
        cpu1802_cycle(&cpu);
        n++;
    } while (cpu.phase != CPU_PHASE_NEXT && cpu.phase != CPU_PHASE_IDLE);
    return n;
}

/* Put a program at 0, set up D, DF and the byte at M(R(2)), run one
 * instruction. */
static int run1(const uint8_t *prog, int n, uint8_t d, bool df, uint8_t m)
{
    setup();
    load_program(prog, n);
    cpu.x = 2;
    cpu.d = d;
    cpu.df = df;
    mem[STACK_AT] = m;
    return step();
}

/* ------------------------------------------------------------------ */
/*  Instruction tests                                                   */
/* ------------------------------------------------------------------ */
/* Author: Thomas Dzubin */
static void test_arithmetic(void)
{
    static const uint8_t add[]  = { 0xF4 }, adc[] = { 0x74 };
    static const uint8_t sd[]   = { 0xF5 }, sdb[] = { 0x75 };
    static const uint8_t sm[]   = { 0xF7 }, smb[] = { 0x77 };
    static const uint8_t adi[]  = { 0xFC, 0x01 };
    static const uint8_t smi[]  = { 0xFF, 0x01 };

    CHECK(run1(add, 1, 0x12, false, 0x34) == 2 && cpu.d == 0x46 && !cpu.df);
    run1(add, 1, 0xF0, false, 0x20);
    CHECK(cpu.d == 0x10 && cpu.df);
    run1(adc, 1, 0xFF, true, 0x00);
    CHECK(cpu.d == 0x00 && cpu.df);
    run1(adc, 1, 0x01, false, 0x01);
    CHECK(cpu.d == 0x02 && !cpu.df);

    run1(sd, 1, 0x03, false, 0x05);              /* M - D */
    CHECK(cpu.d == 0x02 && cpu.df);
    run1(sd, 1, 0x05, false, 0x03);
    CHECK(cpu.d == 0xFE && !cpu.df);
    run1(sdb, 1, 0x01, false, 0x10);             /* M - D - borrow */
    CHECK(cpu.d == 0x0E && cpu.df);
    run1(sdb, 1, 0x20, true, 0x10);
    CHECK(cpu.d == 0xF0 && !cpu.df);

    run1(sm, 1, 0x05, false, 0x03);              /* D - M */
    CHECK(cpu.d == 0x02 && cpu.df);
    run1(sm, 1, 0x03, false, 0x05);
    CHECK(cpu.d == 0xFE && !cpu.df);
    run1(smb, 1, 0x10, false, 0x01);
    CHECK(cpu.d == 0x0E && cpu.df);
    run1(smb, 1, 0x10, true, 0x01);
    CHECK(cpu.d == 0x0F && cpu.df);

    run1(adi, 2, 0xFF, false, 0);
    CHECK(cpu.d == 0x00 && cpu.df && cpu.r[0] == 2);
    run1(smi, 2, 0x00, false, 0);
    CHECK(cpu.d == 0xFF && !cpu.df);
}

/* Author: Thomas Dzubin */
static void test_logic_and_shifts(void)
{
    static const uint8_t or_[] = { 0xF1 }, and_[] = { 0xF2 }, xor_[] = { 0xF3 };
    static const uint8_t shr[] = { 0xF6 }, shl[] = { 0xFE };
    static const uint8_t shrc[] = { 0x76 }, shlc[] = { 0x7E };

    run1(or_, 1, 0xF0, false, 0x0F);
    CHECK(cpu.d == 0xFF);
    run1(and_, 1, 0xF0, false, 0x3C);
    CHECK(cpu.d == 0x30);
    run1(xor_, 1, 0xFF, false, 0x0F);
    CHECK(cpu.d == 0xF0);

    run1(shr, 1, 0x03, false, 0);
    CHECK(cpu.d == 0x01 && cpu.df);
    run1(shl, 1, 0x81, false, 0);
    CHECK(cpu.d == 0x02 && cpu.df);
    run1(shrc, 1, 0x02, true, 0);
    CHECK(cpu.d == 0x81 && !cpu.df);
    run1(shlc, 1, 0x40, true, 0);
    CHECK(cpu.d == 0x81 && !cpu.df);
    run1(shlc, 1, 0x80, false, 0);
    CHECK(cpu.d == 0x00 && cpu.df);
}

/* Author: Thomas Dzubin */
static void test_registers_and_memory(void)
{
    static const uint8_t inc5[] = { 0x15 }, dec5[] = { 0x25 };
    static const uint8_t glo[] = { 0x85 }, ghi[] = { 0x95 };
    static const uint8_t plo[] = { 0xA5 }, phi[] = { 0xB5 };
    static const uint8_t lda[] = { 0x45 }, str[] = { 0x55 }, ldn[] = { 0x05 };
    static const uint8_t ldx[] = { 0xF0 }, ldxa[] = { 0x72 }, stxd[] = { 0x73 };
    static const uint8_t irx[] = { 0x60 }, sex[] = { 0xE7 }, sep[] = { 0xD3 };
    static const uint8_t ldi[] = { 0xF8, 0x5A };

    run1(inc5, 1, 0, false, 0);
    CHECK(cpu.r[5] == 1);
    setup(); load_program(inc5, 1); cpu.r[5] = 0xFFFF; step();
    CHECK(cpu.r[5] == 0);
    setup(); load_program(dec5, 1); cpu.r[5] = 0; step();
    CHECK(cpu.r[5] == 0xFFFF);

    setup(); load_program(glo, 1); cpu.r[5] = 0x1234; step();
    CHECK(cpu.d == 0x34);
    setup(); load_program(ghi, 1); cpu.r[5] = 0x1234; step();
    CHECK(cpu.d == 0x12);
    setup(); load_program(plo, 1); cpu.r[5] = 0x1234; cpu.d = 0x99; step();
    CHECK(cpu.r[5] == 0x1299);
    setup(); load_program(phi, 1); cpu.r[5] = 0x1234; cpu.d = 0x99; step();
    CHECK(cpu.r[5] == 0x9934);

    setup(); load_program(lda, 1); cpu.r[5] = 0x0200; mem[0x200] = 0x77; step();
    CHECK(cpu.d == 0x77 && cpu.r[5] == 0x0201);
    setup(); load_program(str, 1); cpu.r[5] = 0x0200; cpu.d = 0x66; step();
    CHECK(mem[0x200] == 0x66 && cpu.r[5] == 0x0200);
    setup(); load_program(ldn, 1); cpu.r[5] = 0x0200; mem[0x200] = 0x55; step();
    CHECK(cpu.d == 0x55 && cpu.r[5] == 0x0200);

    run1(ldx, 1, 0, false, 0x44);
    CHECK(cpu.d == 0x44 && cpu.r[2] == STACK_AT);
    run1(ldxa, 1, 0, false, 0x44);
    CHECK(cpu.d == 0x44 && cpu.r[2] == STACK_AT + 1);
    run1(stxd, 1, 0x33, false, 0);
    CHECK(mem[STACK_AT] == 0x33 && cpu.r[2] == STACK_AT - 1);
    run1(irx, 1, 0, false, 0);
    CHECK(cpu.r[2] == STACK_AT + 1);

    setup(); load_program(sex, 1); step();
    CHECK(cpu.x == 7);
    setup(); load_program(sep, 1); cpu.r[3] = 0x0300; step();
    CHECK(cpu.p == 3);

    setup(); load_program(ldi, 2); step();
    CHECK(cpu.d == 0x5A && cpu.r[0] == 2);
}

/* Author: Thomas Dzubin */
static void test_branches(void)
{
    static const uint8_t br[]  = { 0x30, 0x42 };
    static const uint8_t bz[]  = { 0x32, 0x42 };
    static const uint8_t bnz[] = { 0x3A, 0x42 };
    static const uint8_t bq[]  = { 0x31, 0x42 };
    static const uint8_t b1[]  = { 0x34, 0x42 };
    static const uint8_t bn4[] = { 0x3F, 0x42 };
    static const uint8_t skp[] = { 0x38, 0x42 };
    static const uint8_t lbr[] = { 0xC0, 0x12, 0x34 };
    static const uint8_t lbz[] = { 0xC2, 0x12, 0x34 };
    static const uint8_t lskp[] = { 0xC8, 0x12, 0x34 };
    static const uint8_t lsz[] = { 0xCE, 0x12, 0x34 };
    static const uint8_t lsie[] = { 0xCC, 0x12, 0x34 };
    static const uint8_t nop[] = { 0xC4 };

    CHECK(run1(br, 2, 0, false, 0) == 2 && cpu.r[0] == 0x0042);
    run1(bz, 2, 0, false, 0);
    CHECK(cpu.r[0] == 0x0042);
    run1(bz, 2, 1, false, 0);
    CHECK(cpu.r[0] == 0x0002);
    run1(bnz, 2, 1, false, 0);
    CHECK(cpu.r[0] == 0x0042);

    setup(); load_program(bq, 2); cpu.q = true; step();
    CHECK(cpu.r[0] == 0x0042);
    setup(); load_program(bq, 2); step();
    CHECK(cpu.r[0] == 0x0002);

    /* a flag is true when its pin is LOW: B1 branches on a low pin */
    setup(); load_program(b1, 2); cpu.ef = 0x0E; step();     /* EF1 low  */
    CHECK(cpu.r[0] == 0x0042);
    setup(); load_program(b1, 2); cpu.ef = 0x0F; step();     /* EF1 high */
    CHECK(cpu.r[0] == 0x0002);
    setup(); load_program(bn4, 2); cpu.ef = 0x0F; step();    /* EF4 high */
    CHECK(cpu.r[0] == 0x0042);
    setup(); load_program(bn4, 2); cpu.ef = 0x07; step();    /* EF4 low  */
    CHECK(cpu.r[0] == 0x0002);

    run1(skp, 2, 0, false, 0);
    CHECK(cpu.r[0] == 0x0002);

    CHECK(run1(lbr, 3, 0, false, 0) == 3 && cpu.r[0] == 0x1234);
    run1(lbz, 3, 0, false, 0);
    CHECK(cpu.r[0] == 0x1234);
    run1(lbz, 3, 1, false, 0);
    CHECK(cpu.r[0] == 0x0003);
    run1(lskp, 3, 0, false, 0);
    CHECK(cpu.r[0] == 0x0003);
    run1(lsz, 3, 0, false, 0);
    CHECK(cpu.r[0] == 0x0003);
    run1(lsz, 3, 1, false, 0);
    CHECK(cpu.r[0] == 0x0001);
    setup(); load_program(lsie, 3); step();                  /* IE is set */
    CHECK(cpu.r[0] == 0x0003);
    CHECK(run1(nop, 1, 0, false, 0) == 3 && cpu.r[0] == 1);
}

/* Author: Thomas Dzubin */
static void test_stack_and_io(void)
{
    static const uint8_t mark[] = { 0x79 }, sav[] = { 0x78 };
    static const uint8_t ret[] = { 0x70 }, dis[] = { 0x71 };
    static const uint8_t out4[] = { 0x64 }, inp4[] = { 0x6C };
    static const uint8_t seq[] = { 0x7B }, req[] = { 0x7A };

    setup(); load_program(mark, 1); cpu.x = 2; cpu.p = 0; cpu.r[2] = 0x0200; step();
    CHECK(cpu.t == 0x20 && mem[0x200] == 0x20 && cpu.x == 0 && cpu.r[2] == 0x01FF);

    setup(); load_program(sav, 1); cpu.x = 2; cpu.t = 0xA5; step();
    CHECK(mem[STACK_AT] == 0xA5);

    setup(); load_program(ret, 1); cpu.x = 2; cpu.ie = false; mem[STACK_AT] = 0x31; step();
    CHECK(cpu.x == 3 && cpu.p == 1 && cpu.ie && cpu.r[2] == STACK_AT + 1);
    setup(); load_program(dis, 1); cpu.x = 2; mem[STACK_AT] = 0x31; step();
    CHECK(cpu.x == 3 && cpu.p == 1 && !cpu.ie);

    setup(); load_program(out4, 1); cpu.x = 2; mem[STACK_AT] = 0xAB; step();
    CHECK(out_count == 1 && out_port == 4 && out_data == 0xAB &&
          cpu.r[2] == STACK_AT + 1);
    setup(); load_program(inp4, 1); cpu.x = 2; in_value = 0x5A; step();
    CHECK(mem[STACK_AT] == 0x5A && cpu.d == 0x5A && cpu.r[2] == STACK_AT);

    setup(); load_program(seq, 1); step();
    CHECK(cpu.q);
    setup(); load_program(req, 1); cpu.q = true; step();
    CHECK(!cpu.q);
}

/* Every one of the 256 opcodes runs, and takes 2 machine cycles, or 3 for
 * the long group C0 to CF. */
static void test_all_opcodes(void)
{
    int op;

    for (op = 0; op < 256; op++) {
        int cycles;

        setup();
        mem[0] = (uint8_t)op;
        mem[1] = 0x00;
        mem[2] = 0x10;
        cpu.x = 2;
        cpu.r[1] = cpu.r[3] = 0x0300;
        cpu.r[2] = STACK_AT;
        cycles = step();
        CHECK(cycles == ((op >> 4) == 0xC ? 3 : 2));
    }
}

/* ------------------------------------------------------------------ */
/*  Sequencing: interrupt, DMA, idle, reset, load                       */
/* ------------------------------------------------------------------ */
/* Author: Thomas Dzubin */
static void test_sequencing(void)
{
    static const uint8_t nops[] = { 0xE0, 0xE0, 0xE0, 0xE0 };   /* SEX 0 */

    /* An interrupt is taken after the instruction in progress. */
    setup(); load_program(nops, 4); cpu.x = 1; cpu.p = 0;
    cpu1802_cycle(&cpu);                     /* S0 of SEX 0 */
    cpu.int_req = true;
    CHECK(cpu1802_cycle(&cpu) == CPU_CYCLE_S1);
    CHECK(cpu1802_cycle(&cpu) == CPU_CYCLE_S3);
    CHECK(cpu.t == 0x00 && cpu.x == 2 && cpu.p == 1 && !cpu.ie);
    cpu.int_req = false;
    CHECK(cpu1802_cycle(&cpu) == CPU_CYCLE_S0);

    /* With IE clear it is not taken. */
    setup(); load_program(nops, 4); cpu.ie = false; cpu.int_req = true;
    CHECK(step() == 2);
    CHECK(cpu1802_cycle(&cpu) == CPU_CYCLE_S0);

    /* The first cycle after reset is never an interrupt. */
    cpu1802_power_on(&cpu);
    cpu.mem_read = t_read; cpu.mem_write = t_write; cpu.io_in = t_in;
    cpu.io_out = t_out; cpu.dma_in = t_dma_in; cpu.dma_out = t_dma_out;
    cpu.mode = CPU_MODE_RESET; cpu1802_cycle(&cpu);
    cpu.mode = CPU_MODE_RUN; cpu.int_req = true;
    CHECK(cpu1802_cycle(&cpu) == CPU_CYCLE_INIT);
    CHECK(cpu1802_cycle(&cpu) == CPU_CYCLE_S0);

    /* DMA-OUT reads M(R0), steps R0, and waits for the instruction to end. */
    setup(); load_program(nops, 4); mem[0x0500] = 0xC3;
    cpu.p = 3; cpu.r[3] = 0; cpu.r[0] = 0x0500;      /* the PC is not R0 */
    CHECK(cpu1802_cycle(&cpu) == CPU_CYCLE_S0);
    cpu.dma_out_req = true;
    CHECK(cpu1802_cycle(&cpu) == CPU_CYCLE_S1);      /* the instruction ends first */
    CHECK(cpu1802_cycle(&cpu) == CPU_CYCLE_S2);
    CHECK(dma_out_count == 1 && dma_out_data == 0xC3 && cpu.r[0] == 0x0501);
    cpu.dma_out_req = false;

    /* DMA-IN writes the supplied byte at R0, and beats DMA-OUT. */
    setup(); load_program(nops, 4); cpu.p = 3; cpu.r[3] = 0; cpu.r[0] = 0x0600;
    dma_in_value = 0x99;
    cpu.dma_in_req = true; cpu.dma_out_req = true;
    CHECK(cpu1802_cycle(&cpu) == CPU_CYCLE_S2);
    CHECK(mem[0x0600] == 0x99 && cpu.r[0] == 0x0601 && dma_out_count == 0);
    cpu.dma_in_req = false;
    CHECK(cpu1802_cycle(&cpu) == CPU_CYCLE_S2 && dma_out_count == 1);
    cpu.dma_out_req = false;

    /* A long instruction's second cycle is never interrupted. */
    {
        static const uint8_t lbr[] = { 0xC4, 0xE0 };
        setup(); load_program(lbr, 2);
        cpu1802_cycle(&cpu);                 /* S0 */
        cpu.int_req = true;
        CHECK(cpu1802_cycle(&cpu) == CPU_CYCLE_S1);
        CHECK(cpu1802_cycle(&cpu) == CPU_CYCLE_S1);
        CHECK(cpu1802_cycle(&cpu) == CPU_CYCLE_S3);
    }

    /* IDL: S1 over and over until a request; a DMA does not end it. */
    {
        static const uint8_t idl[] = { 0x00, 0xE0 };
        setup(); load_program(idl, 2);
        CHECK(step() == 2);
        CHECK(cpu1802_cycle(&cpu) == CPU_CYCLE_S1);
        CHECK(cpu1802_cycle(&cpu) == CPU_CYCLE_S1);
        cpu.r[3] = 0; cpu.r[0] = 0x0700; cpu.dma_in_req = true;
        CHECK(cpu1802_cycle(&cpu) == CPU_CYCLE_S2);
        cpu.dma_in_req = false;
        CHECK(cpu1802_cycle(&cpu) == CPU_CYCLE_S1);
        cpu.int_req = true;
        CHECK(cpu1802_cycle(&cpu) == CPU_CYCLE_S3);
        cpu.int_req = false;
        CHECK(cpu1802_cycle(&cpu) == CPU_CYCLE_S0);
    }

    /* Load mode: no cycles unless something is requested. */
    setup(); cpu.mode = CPU_MODE_LOAD; cpu.r[0] = 0x0010; dma_in_value = 0x42;
    CHECK(cpu1802_cycle(&cpu) == CPU_CYCLE_NONE);
    cpu.dma_in_req = true;
    CHECK(cpu1802_cycle(&cpu) == CPU_CYCLE_S2);
    CHECK(mem[0x10] == 0x42 && cpu.r[0] == 0x0011);
    cpu.dma_in_req = false;
    CHECK(cpu1802_cycle(&cpu) == CPU_CYCLE_NONE);
    cpu.mode = CPU_MODE_PAUSE;
    CHECK(cpu1802_cycle(&cpu) == CPU_CYCLE_NONE);
}

/* ------------------------------------------------------------------ */
/*  The board: front panel, load mode, and the video chip               */
/* ------------------------------------------------------------------ */
static void type_byte(elf_t *m, unsigned hi, unsigned lo)
{
    elf_key_hex(m, hi);
    elf_key_hex(m, lo);
    elf_in_button(m, true);
    elf_run(m, 4);
    elf_in_button(m, false);
    elf_run(m, 4);
}

/* Author: Thomas Dzubin */
static void test_front_panel(void)
{
    static elf_t m;

    elf_init(&m);
    elf_set_switches(&m, false, true, false);          /* LOAD */
    elf_run(&m, 4);
    CHECK(m.cpu.r[0] == 0);
    type_byte(&m, 0x1, 0x2);
    type_byte(&m, 0x3, 0x4);
    CHECK(m.ram[0] == 0x12 && m.ram[1] == 0x34 && m.cpu.r[0] == 2);
    CHECK(m.display == 0x34);

    elf_set_switches(&m, false, true, true);           /* memory protect */
    type_byte(&m, 0x5, 0x6);
    CHECK(m.ram[2] == 0x00 && m.cpu.r[0] == 3);

    /* RESET clears R0; RUN then starts at 0000: SEQ, then idle in a loop. */
    elf_set_switches(&m, false, false, false);
    elf_run(&m, 4);
    elf_set_switches(&m, false, true, false);
    elf_run(&m, 4);
    CHECK(m.cpu.r[0] == 0);
    m.ram[0] = 0x7B;                                   /* SEQ */
    m.ram[1] = 0x30; m.ram[2] = 0x01;                  /* BR 01 */
    elf_set_switches(&m, true, false, false);          /* RUN */
    elf_run(&m, 20);
    CHECK(m.cpu.q && m.q_changes == 1);
    CHECK(m.cpu.mode == CPU_MODE_RUN);
}

/* The standard way to run the 1861 on an Elf: an interrupt routine that sets
 * R0 to the picture and uses exactly the 29 cycles between the interrupt and
 * the first DMA burst, and a main program of nothing but two-cycle
 * instructions. The picture must come out of the chip exactly as stored.
 *
 * Author: Thomas Dzubin */
static void test_video(void)
{
    static elf_t m;
    static const uint8_t boot[] = {
        0xF8, 0x00, 0xB1, 0xF8, ISR_AT, 0xA1,   /* R1 = interrupt routine    */
        0xF8, 0x00, 0xB2, 0xF8, 0xFF, 0xA2,     /* R2 = stack               */
        0xF8, 0x00, 0xB3, 0xF8, 0x17, 0xA3,     /* R3 = the main program    */
        0xE2,                                   /* X = 2                    */
        0x69,                                   /* INP 1: display on        */
        0xD3,                                   /* P = 3 (R0 is the DMA pointer) */
        0x00, 0x00,
        0x30, 0x17                              /* 0017: BR *               */
    };
    /* The routine starts with the RET that ends the previous interrupt, so
     * that R1 is back at the entry point (ISR_AT) for the next one. */
    static const uint8_t isr[] = {
        0x70,                                   /* ISR_AT - 1: RET          */
        0x22, 0x78,                             /* DEC 2, SAV      (4)      */
        0xC4,                                   /* NOP             (3)      */
        0xF8, IMAGE_AT >> 8, 0xB0,              /* R0.1            (4)      */
        0xF8, IMAGE_AT & 0xFF, 0xA0,            /* R0.0            (4)      */
        0xE2, 0xE2, 0xE2, 0xE2, 0xE2, 0xE2, 0xE2,   /* 7 x SEX 2   (14)     */
        0x30, ISR_AT - 1                        /* BR to the RET            */
    };
    int row, col;
    uint32_t cycle;
    int efx_cycles = 0, int_cycles = 0, dma_cycles = 0;
    pixie_t counter;

    /* the frame's pin counts, from the chip alone */
    pixie_init(&counter);
    pixie_display_on(&counter);
    for (cycle = 0; cycle < PIXIE_LINES_PER_FRAME * PIXIE_CYCLES_PER_LINE; cycle++) {
        efx_cycles += pixie_efx_low(&counter);
        int_cycles += pixie_int_request(&counter);
        dma_cycles += pixie_dma_request(&counter);
        pixie_cycle_done(&counter);
    }
    CHECK(efx_cycles == BLANK_LINES * PIXIE_CYCLES_PER_LINE);
    CHECK(int_cycles == 2 * PIXIE_CYCLES_PER_LINE);
    CHECK(dma_cycles == PIXIE_DISPLAY_LINES * PIXIE_BYTES_PER_LINE);

    elf_init(&m);
    memcpy(m.ram, boot, sizeof boot);
    memcpy(m.ram + ISR_AT - 1, isr, sizeof isr);
    for (row = 0; row < PIXIE_DISPLAY_LINES; row++)
        for (col = 0; col < PIXIE_BYTES_PER_LINE; col++)
            m.ram[IMAGE_AT + row * PIXIE_BYTES_PER_LINE + col] =
                (uint8_t)(row * 8 + col + 1) ^ 0x5A;

    elf_set_switches(&m, true, false, false);          /* RUN */
    elf_run(&m, VIDEO_FRAMES * PIXIE_LINES_PER_FRAME * PIXIE_CYCLES_PER_LINE);

    CHECK(m.video.on);
    CHECK(m.video.frames >= VIDEO_FRAMES - 2);
    for (row = 0; row < PIXIE_DISPLAY_LINES; row++)
        for (col = 0; col < PIXIE_BYTES_PER_LINE; col++)
            if (m.video.bits[row][col] != m.ram[IMAGE_AT + row * 8 + col]) {
                CHECK(0 && "picture byte differs");
                row = PIXIE_DISPLAY_LINES;
                break;
            }
}

/* The RAM ends exactly at ELF_RAM_SIZE: the last byte works, the next address
 * reads as nothing and ignores writes. */
/* A tiny fake SD card for the ROM images: a few files in memory. */
static struct { char name[16]; uint8_t data[ROM_SLOT_SIZE + 16]; uint32_t len; } card[4];
static int card_files;

int plat_file_save(const char *name, const uint8_t *data, uint32_t len)
{
    int i;

    for (i = 0; i < card_files; i++)
        if (strcmp(card[i].name, name) == 0)
            break;
    if (i == card_files)
        card_files++;
    snprintf(card[i].name, sizeof card[i].name, "%s", name);
    memcpy(card[i].data, data, len);
    card[i].len = len;
    return PLAT_FILE_OK;
}

int plat_file_load(const char *name, uint8_t *data, uint32_t max, uint32_t *len)
{
    int i;

    *len = 0;
    for (i = 0; i < card_files; i++)
        if (strcmp(card[i].name, name) == 0) {
            *len = card[i].len < max ? card[i].len : max;
            memcpy(data, card[i].data, *len);
            return PLAT_FILE_OK;
        }
    return PLAT_FILE_NOT_FOUND;
}

/* ROM images: fitted over the Elf's memory, read-only, remembered.
 *
 * Author: Thomas Dzubin */
static void test_rom(void)
{
    static elf_t m;
    static uint8_t image[1000];
    program_target_t t;
    const rom_info_t *r;
    int i;

    for (i = 0; i < (int)sizeof image; i++)
        image[i] = (uint8_t)(i * 7 + 3);
    plat_file_save("MON.ROM", image, sizeof image);
    plat_file_save("EMPTY.ROM", image, 0);

    CHECK(rom_set(0, "MON.ROM", 0x8000) == ROM_OK);
    r = rom_info(0);
    CHECK(r->fitted && r->address == 0x8000 && r->size == 1024 && strcmp(r->name, "MON.ROM") == 0);
    CHECK(rom_set(1, "MON.ROM", 0x8010) == ROM_ERR_ADDRESS && !rom_info(1)->fitted);   /* not on a page */
    CHECK(rom_set(1, "MON.ROM", 0xFE00) == ROM_ERR_ADDRESS);                         /* runs off the end */
    CHECK(rom_set(1, "NONE.ROM", 0xC000) == PLAT_FILE_NOT_FOUND);
    CHECK(rom_set(1, "EMPTY.ROM", 0xC000) == ROM_ERR_EMPTY);
    CHECK(rom_set(2, "MON.ROM", 0) == ROM_ERR_SLOT && rom_info(2) == NULL);
    CHECK(rom_set(1, "MON.ROM", 0xC000) == ROM_OK);

    /* the Elf fits them when it is powered on: read-only, and below them RAM as before */
    elf_init(&m);
    CHECK(elf_peek(&m, 0x8000) == 3 && elf_peek(&m, 0x8001) == 10 && elf_peek(&m, 0xC3E7) == image[999]);
    CHECK(m.cpu.mem_read(&m, 0x8002) == image[2]);
    m.cpu.mem_write(&m, 0x8002, 0xAA);
    CHECK(m.cpu.mem_read(&m, 0x8002) == image[2]);                  /* a ROM does not take a write */
    CHECK(elf_peek(&m, 0x83E8) == 0);                               /* padded with zeros to a page */
    CHECK(elf_peek(&m, 0x8400) == ELF_UNMAPPED_READ);               /* nothing past it */
    m.cpu.mem_write(&m, 0x0010, 0x77);
    CHECK(m.ram[0x10] == 0x77);                                     /* the RAM is as it was */

    /* taking one out, and the memory map made again */
    elf_program_target(&m, &t);
    CHECK(t.bus == &m.bus && t.remap != NULL);
    rom_clear(0);
    t.remap(t.ctx);
    CHECK(elf_peek(&m, 0x8000) == ELF_UNMAPPED_READ && elf_peek(&m, 0xC000) == 3);
    elf_set_switches(&m, false, false, true);                       /* memory protect stays across a remap */
    t.remap(t.ctx);
    m.cpu.mem_write(&m, 0x0010, 0x11);
    CHECK(m.ram[0x10] == 0x77);

    /* remembered in a config file, and fitted again from it */
    rom_config_save();
    rom_clear(1);
    CHECK(!rom_info(1)->fitted);
    rom_config_load();
    CHECK(rom_info(1)->fitted && rom_info(1)->address == 0xC000 && !rom_info(0)->fitted);
    rom_clear(1);
    rom_config_save();
    rom_config_load();
    CHECK(!rom_info(0)->fitted && !rom_info(1)->fitted);
}

/* The memory map: RAM, ROM, a device on a page, write protection, nothing. */
static int mb_reads, mb_writes;
static uint8_t mb_last;

static uint8_t mb_device_read(void *ctx, uint16_t addr)
{
    (void)ctx;
    mb_reads++;
    return (uint8_t)(addr & 0xFF) ^ 0x5A;
}

static void mb_device_write(void *ctx, uint16_t addr, uint8_t data)
{
    (void)ctx;
    (void)addr;
    mb_writes++;
    mb_last = data;
}

/* Author: Thomas Dzubin */
static void test_membus(void)
{
    static membus_t bus;
    static uint8_t ram[1024], rom[512], shadow[256];
    uint32_t i;

    membus_init(&bus, 0xFF);
    membus_map_ram(&bus, 0x0000, sizeof ram, ram);
    membus_map_rom(&bus, 0xF000, sizeof rom, rom);
    membus_map_io(&bus, 0xC000, 256, mb_device_read, mb_device_write, NULL);
    membus_map_hooked(&bus, 0x8000, sizeof shadow, shadow, mb_device_write, NULL);
    for (i = 0; i < sizeof rom; i++)
        rom[i] = (uint8_t)(i * 3);

    /* RAM reads back what was written, across all its pages */
    membus_write(&bus, 0x0000, 0x11);
    membus_write(&bus, 0x03FF, 0x22);
    CHECK(membus_read(&bus, 0x0000) == 0x11 && membus_read(&bus, 0x03FF) == 0x22);
    CHECK(ram[0] == 0x11 && ram[1023] == 0x22);
    CHECK(membus_read(&bus, 0x0400) == 0xFF);                /* just past it: nothing */
    membus_write(&bus, 0x0400, 0x33);                        /* ignored */
    CHECK(membus_read(&bus, 0x0400) == 0xFF);

    /* ROM is read, and writes to it do nothing */
    CHECK(membus_read(&bus, 0xF000) == rom[0] && membus_read(&bus, 0xF1FF) == rom[511]);
    membus_write(&bus, 0xF010, 0x99);
    CHECK(rom[0x10] == 0x30);
    CHECK(membus_read(&bus, 0xF200) == 0xFF);                /* past the ROM */

    /* a device sees every access, with the full address */
    mb_reads = mb_writes = 0;
    CHECK(membus_read(&bus, 0xC012) == (0x12 ^ 0x5A) && mb_reads == 1);
    membus_write(&bus, 0xC0FF, 0x77);
    CHECK(mb_writes == 1 && mb_last == 0x77);
    CHECK(membus_peek(&bus, 0xC012) == 0xFF && mb_reads == 1);   /* a peek touches nothing */

    /* memory that is read directly and whose writes go to a hook */
    shadow[5] = 0x42;
    mb_writes = 0;
    CHECK(membus_read(&bus, 0x8005) == 0x42 && membus_peek(&bus, 0x8005) == 0x42);
    membus_write(&bus, 0x8005, 0x24);
    CHECK(mb_writes == 1 && mb_last == 0x24 && shadow[5] == 0x42);   /* the hook decides */

    /* write protect: only the RAM pages asked for, and back again */
    membus_write_protect(&bus, 0x0000, 0x0100, true);
    membus_write(&bus, 0x0000, 0xAA);
    membus_write(&bus, 0x0100, 0xBB);
    CHECK(ram[0] == 0x11 && ram[0x100] == 0xBB);
    membus_write_protect(&bus, 0x0000, 0x0100, false);
    membus_write(&bus, 0x0000, 0xAA);
    CHECK(ram[0] == 0xAA);
    membus_write_protect(&bus, 0xC000, 256, true);           /* not RAM: unchanged */
    mb_writes = 0;
    membus_write(&bus, 0xC000, 1);
    CHECK(mb_writes == 1);

    /* unmapping, and a range that runs off the end of the address space */
    membus_unmap(&bus, 0x0000, 0x0100);
    CHECK(membus_read(&bus, 0x0000) == 0xFF);
    membus_map_ram(&bus, 0xFF00, 0x400, ram);                /* only the last page fits */
    membus_write(&bus, 0xFFFF, 0x5C);
    CHECK(ram[255] == 0x5C);

    /* the Elf as a machine for the shell: its clock, and running it by the descriptor */
    {
        static elf_t em;
        bool hit = false;

        elf_init(&em);
        CHECK(elf_machine.clock_hz == ELF_CYCLES_PER_SEC && elf_machine.clock_hz > 200000u);
        CHECK(elf_machine.run(&em, 100) == 0);                   /* held in RESET: no time passes */
        elf_set_switches(&em, true, false, false);
        CHECK(elf_machine.run(&em, 100) == 100);
        CHECK(elf_machine.run_to != NULL && elf_machine.run_to(&em, 10, 0x0000, &hit) > 0);
    }

    /* the program memory as the editor and the file menu see it */
    {
        static elf_t em;
        program_target_t t;
        static const uint8_t prog[] = { 1, 2, 3 };
        char note[40];

        elf_init(&em);
        elf_program_target(&em, &t);
        CHECK(t.ram == em.ram && t.size == ELF_RAM_SIZE && t.isa == &isa_1802);
        em.ram[100] = 0x55;
        em.vdu[5] = 0x66;
        program_target_load(&t, prog, sizeof prog);             /* a new program: the rest cleared */
        CHECK(em.ram[0] == 1 && em.ram[2] == 3 && em.ram[100] == 0 && em.vdu[5] == 0);
        program_target_load(&t, NULL, 0);
        CHECK(em.ram[0] == 0);
        program_target_load(&t, em.ram, 10);                    /* (a file read into the RAM itself) */
        CHECK(t.extra_key == 'y' && strstr(t.extra_text(t.ctx), "FITTED") != NULL);
        CHECK(t.extra_press(t.ctx, note, sizeof note) && !em.video_installed &&
              strstr(t.extra_text(t.ctx), "NONE") != NULL && strstr(note, "BASIC") != NULL);
    }
}

static void test_ram_edge(void)
{
    static elf_t m;

    elf_init(&m);
    m.cpu.mem_write(&m, ELF_RAM_SIZE - 1, 0xA5);
    CHECK(m.ram[ELF_RAM_SIZE - 1] == 0xA5);
    CHECK(m.cpu.mem_read(&m, ELF_RAM_SIZE - 1) == 0xA5);
    m.cpu.mem_write(&m, ELF_RAM_SIZE, 0x5A);
    CHECK(m.cpu.mem_read(&m, ELF_RAM_SIZE) == ELF_UNMAPPED_READ);
    CHECK(m.cpu.mem_read(&m, 0xFFFF) == ELF_UNMAPPED_READ);
    elf_load_image(&m, m.ram, ELF_RAM_SIZE + 100);      /* too long: cut */
    CHECK(m.ram[ELF_RAM_SIZE - 1] == 0xA5);
}

/* ------------------------------------------------------------------ */
/*  The ASCII keyboard, the VDU and Intel HEX loading                   */
/* ------------------------------------------------------------------ */
/* Author: Thomas Dzubin */
static void test_ascii_keyboard(void)
{
    static elf_t m;
    int i, accepted = 0;
    /* SEX 2, wait for a key (BN3 loops while EF3 is low), INP 7, loop */
    static const uint8_t prog[] = { 0xE2, 0x3E, 0x01, 0x6F, 0x30, 0x01 };

    elf_init(&m);
    for (i = 0; i < ELF_KBD_FIFO + 4; i++)
        if (elf_ascii_key(&m, (uint8_t)('A' + i)))
            accepted++;
    CHECK(accepted == ELF_KBD_FIFO + 1);               /* the latch and the buffer */
    CHECK(m.cpu.io_in(&m, ELF_PORT_ASCII) == 'A');
    CHECK(m.cpu.io_in(&m, ELF_PORT_ASCII) == 'A');     /* still there to read again */
    CHECK(!m.kbd_ready);                               /* EF3 is low now           */

    /* the next key comes up only after the gap, with the machine running */
    elf_set_switches(&m, true, false, false);
    elf_run(&m, 1);                                    /* the init cycle           */
    elf_run(&m, ELF_KBD_GAP_CYCLES - 10);
    CHECK(!m.kbd_ready);
    elf_run(&m, 20);
    CHECK(m.kbd_ready && m.cpu.io_in(&m, ELF_PORT_ASCII) == 'B');

    /* a program can wait on EF3 and read the key */
    elf_init(&m);
    memcpy(m.ram, prog, sizeof prog);
    elf_set_switches(&m, true, false, false);
    elf_run(&m, 1);
    m.cpu.r[2] = 0x0100;
    elf_run(&m, 40);
    CHECK((m.cpu.ef & CPU_EF3) != 0);                  /* nothing waiting: pin high */
    CHECK(m.cpu.d == 0);                               /* still looping            */
    elf_ascii_key(&m, 'K');
    elf_cycle(&m);
    CHECK((m.cpu.ef & CPU_EF3) == 0);                  /* a key waits: pin low     */
    elf_run(&m, 40);
    CHECK(m.cpu.d == 'K' && m.ram[0x100] == 'K' && !m.kbd_ready);
}

/* Author: Thomas Dzubin */
static void test_vdu(void)
{
    static elf_t m;

    elf_init(&m);
    m.cpu.mem_write(&m, ELF_VDU_BASE, 0x4F);
    m.cpu.mem_write(&m, ELF_VDU_BASE + ELF_VDU_SIZE - 1, 0x21);
    CHECK(m.vdu[0] == 0x4F && m.vdu[ELF_VDU_SIZE - 1] == 0x21);
    CHECK(m.cpu.mem_read(&m, ELF_VDU_BASE) == 0x4F);
    CHECK(m.vdu_writes == 2);
    m.cpu.mem_write(&m, ELF_VDU_BASE + ELF_VDU_SIZE, 0x55);      /* not mapped */
    CHECK(m.cpu.mem_read(&m, ELF_VDU_BASE + ELF_VDU_SIZE) == ELF_UNMAPPED_READ);
    CHECK(m.vdu_writes == 2);

    elf_set_switches(&m, m.sw_run, m.sw_load, true);    /* memory protect leaves the VDU alone */
    m.cpu.mem_write(&m, ELF_VDU_BASE + 1, 0x77);
    m.cpu.mem_write(&m, 0x0010, 0x77);
    CHECK(m.vdu[1] == 0x77 && m.ram[0x10] == 0);

    elf_load_image(&m, (const uint8_t *)"x", 1);   /* a new program clears it */
    CHECK(m.vdu[0] == 0 && m.vdu[1] == 0);
}

/* Build one HEX record into out and return its length. */
static int make_record(char *out, unsigned addr, unsigned type,
                       const uint8_t *data, int n, int bad_sum)
{
    unsigned sum = (unsigned)n + (addr >> 8) + (addr & 0xFF) + type;
    int len, i;

    len = sprintf(out, ":%02X%04X%02X", (unsigned)n, addr, type);
    for (i = 0; i < n; i++) {
        len += sprintf(out + len, "%02X", data[i]);
        sum += data[i];
    }
    len += sprintf(out + len, "%02X\r\n", (unsigned)((0x100 - (sum & 0xFF) + bad_sum) & 0xFF));
    return len;
}

/* Author: Thomas Dzubin */
static void test_hex(void)
{
    static uint8_t image[ELF_RAM_SIZE];
    static const uint8_t d1[] = { 0x11, 0x22, 0x33 };
    static const uint8_t d2[] = { 0x44, 0x55 };
    static const uint8_t seg[] = { 0x01, 0x00 };         /* segment 0100: +0x1000 */
    char text[512];
    int n = 0;
    uint32_t stored, skipped;

    memset(image, 0, sizeof image);
    n += make_record(text + n, 0x0010, 0, d1, 3, 0);
    n += make_record(text + n, 0x8000, 0, d2, 2, 0);     /* above the RAM */
    n += make_record(text + n, 0x0000, 2, seg, 2, 0);
    n += make_record(text + n, 0x0020, 0, d2, 2, 0);     /* 1020 */
    n += make_record(text + n, 0, 1, NULL, 0, 0);
    CHECK(hex_load(text, (uint32_t)n, image, sizeof image, &stored, &skipped) == HEX_OK);
    CHECK(image[0x10] == 0x11 && image[0x11] == 0x22 && image[0x12] == 0x33);
    CHECK(image[0x1020] == 0x44 && image[0x1021] == 0x55);
    CHECK(stored == 5 && skipped == 2);

    /* garbage before the colon, LF endings, no EOF record */
    memset(image, 0, sizeof image);
    n = make_record(text, 0x0000, 0, d1, 3, 0);
    text[n - 2] = '\n';                                  /* CRLF to LF */
    memmove(text + 6, text, (size_t)n - 1);
    memcpy(text, "junk\n", 5);
    CHECK(hex_load(text, (uint32_t)n + 4, image, sizeof image, &stored, &skipped) == HEX_OK);
    CHECK(image[0] == 0x11 && image[2] == 0x33);

    n = make_record(text, 0x0000, 0, d1, 3, 1);          /* wrong checksum */
    CHECK(hex_load(text, (uint32_t)n, image, sizeof image, &stored, &skipped) == HEX_ERR_CHECKSUM);
    CHECK(hex_load(":0300", 5, image, sizeof image, &stored, &skipped) == HEX_ERR_FORMAT);
    CHECK(hex_load(":03000000ZZ2233", 15, image, sizeof image, &stored, &skipped) == HEX_ERR_FORMAT);
    CHECK(hex_load("", 0, image, sizeof image, &stored, &skipped) == HEX_OK && stored == 0);
}

/* A program that toggles Q in two equal count-down loops: the machine cycles
 * between rises of Q (q_period) must be exact and steady, so a tone made from
 * it does not warble.
 *
 * Author: Thomas Dzubin */
static void test_q_period(void)
{
    static elf_t m;
    static const uint8_t prog[] = {
        0x7B, 0xF8, 0x29, 0xA5, 0x25, 0x85, 0x3A, 0x04,     /* SEQ, count 41 down */
        0x7A, 0xF8, 0x29, 0xA5, 0x25, 0x85, 0x3A, 0x0C,     /* REQ, count 41 down */
        0x30, 0x00
    };
    uint32_t first, i;

    elf_init(&m);
    memcpy(m.ram, prog, sizeof prog);
    elf_set_switches(&m, true, false, false);
    elf_run(&m, ELF_CYCLES_PER_SEC / 10);
    CHECK(m.q_period > 0);
    first = m.q_period;
    for (i = 0; i < 20; i++) {
        elf_run(&m, 777);                       /* not a multiple of the period */
        CHECK(m.q_period == first);
    }
    CHECK(first >= 500 && first <= 515);        /* about 442 Hz */
    CHECK(ELF_CYCLES_PER_SEC / first >= 430 && ELF_CYCLES_PER_SEC / first <= 450);
}

/* Run the machine for exactly "cycles" machine cycles, collecting every sample
 * of Q's sound that comes out (the FIFO is read often, as the front end does).
 * Returns the number of samples. */
static uint32_t run_collect(elf_t *m, uint32_t cycles, int16_t *out, uint32_t max)
{
    uint32_t total = 0;

    while (cycles > 0) {
        uint32_t chunk = cycles < 1000 ? cycles : 1000;
        uint32_t done = elf_run(m, chunk);

        if (done == 0)
            break;
        cycles -= done;
        if (total < max)
            total += qaudio_read(&m->audio, out + total, max - total);
    }
    return total;
}

/* Q as sound: a sample for every 1/22050 s of machine time exactly, a tone
 * that has the pitch of Q's changes, silence for a steady Q.
 *
 * Author: Thomas Dzubin */
static void test_q_audio(void)
{
    static elf_t m;
    static int16_t samples[QAUDIO_RATE + 64];
    static const uint8_t tone[] = {
        0x7B, 0xF8, 0x29, 0xA5, 0x25, 0x85, 0x3A, 0x04,     /* SEQ, count 41 down */
        0x7A, 0xF8, 0x29, 0xA5, 0x25, 0x85, 0x3A, 0x0C,     /* REQ, count 41 down */
        0x30, 0x00
    };
    static const uint8_t steady[] = { 0x7B, 0x30, 0x01 };   /* SEQ, then wait */
    uint32_t n, i, rises = 0;
    int16_t lo = 0, hi = 0;

    elf_init(&m);
    memcpy(m.ram, tone, sizeof tone);
    elf_set_switches(&m, true, false, false);
    n = run_collect(&m, ELF_CYCLES_PER_SEC, samples, QAUDIO_RATE + 64);
    CHECK(n == QAUDIO_RATE);                    /* exactly one second of samples */
    for (i = 1000; i < n; i++) {                /* skip the start-up */
        if (samples[i] < lo) lo = samples[i];
        if (samples[i] > hi) hi = samples[i];
        if (samples[i - 1] < 0 && samples[i] >= 0)
            rises++;
    }
    CHECK(hi > 8000 && lo < -8000);             /* a loud tone, centred on 0 */
    CHECK(rises >= 415 && rises <= 430);        /* 442 Hz, less the first 1000 samples */

    /* nobody reads: the FIFO fills up and the extra samples are dropped */
    elf_init(&m);
    memcpy(m.ram, tone, sizeof tone);
    elf_set_switches(&m, true, false, false);
    elf_run(&m, ELF_CYCLES_PER_SEC / 2);
    CHECK(m.audio.count == QAUDIO_FIFO);
    qaudio_flush(&m.audio);
    CHECK(m.audio.count == 0);

    /* Q stuck high is not a sound: the DC blocker takes it out */
    elf_init(&m);
    memcpy(m.ram, steady, sizeof steady);
    elf_set_switches(&m, true, false, false);
    n = run_collect(&m, ELF_CYCLES_PER_SEC / 4, samples, QAUDIO_RATE);
    CHECK(n > 5000);
    hi = 0;
    for (i = n - 200; i < n; i++)
        if (samples[i] > hi || -samples[i] > hi)
            hi = samples[i] < 0 ? (int16_t)-samples[i] : samples[i];
    CHECK(hi < 40);

    /* a stopped machine makes no samples */
    elf_init(&m);
    elf_run(&m, 5000);
    CHECK(m.audio.count == 0);
}

/* Single-stepping and breakpoints.
 *
 * Author: Thomas Dzubin */
static void test_step_and_break(void)
{
    static elf_t m;
    static const uint8_t prog[] = {
        0xF8, 0x2F,                             /* 0000 LDI 2F       */
        0xA5,                                   /* 0002 PLO 5        */
        0xC0, 0x00, 0x08,                       /* 0003 LBR 0008     */
        0x00, 0x00,
        0x7B,                                   /* 0008 SEQ          */
        0x30, 0x08                              /* 0009 BR 0008      */
    };
    static const uint8_t count[] = {
        0xF8, 0x01,                             /* 0000 LDI 1        */
        0xFC, 0x01,                             /* 0002 ADI 1        */
        0x30, 0x02                              /* 0004 BR 0002      */
    };
    bool hit;
    uint32_t n;

    elf_init(&m);
    memcpy(m.ram, prog, sizeof prog);
    CHECK(elf_step(&m) == 0);                   /* in RESET no time passes */
    elf_set_switches(&m, true, false, false);
    CHECK(elf_step(&m) == 3);                   /* the init cycle and LDI  */
    CHECK(m.cpu.d == 0x2F && m.cpu.r[0] == 2);
    CHECK(elf_step(&m) == 2 && m.cpu.r[5] == 0x002F);
    CHECK(elf_step(&m) == 3 && m.cpu.r[0] == 8);        /* a long branch: 3 */
    CHECK(elf_step(&m) == 2 && m.cpu.q);

    elf_init(&m);
    memcpy(m.ram, count, sizeof count);
    elf_set_switches(&m, true, false, false);
    n = elf_run_to(&m, 1000, 0x0004, &hit);
    CHECK(hit && m.cpu.r[0] == 4 && m.cpu.d == 2 && n < 20);
    n = elf_run_to(&m, 1000, 0x0004, &hit);     /* from the breakpoint: goes on */
    CHECK(hit && m.cpu.r[0] == 4 && m.cpu.d == 3 && n > 2);
    n = elf_run_to(&m, 1000, 0x1000, &hit);     /* never reached */
    CHECK(!hit && n == 1000);
    CHECK(elf_peek(&m, 0x0001) == 0x01 && elf_peek(&m, 0x5000) == 0x00);
}

/* The disassembler: spelling, operands and lengths.
 *
 * Author: Thomas Dzubin */
static void test_disasm(void)
{
    char text[24];
    uint8_t b[3];
    unsigned op;

#define DIS(addr, b0, b1, b2, expected, len) do { \
        b[0] = (b0); b[1] = (b1); b[2] = (b2); \
        CHECK(dis1802((addr), b, text, sizeof text) == (len)); \
        CHECK(strcmp(text, (expected)) == 0); \
    } while (0)

    DIS(0x0000, 0xF8, 0x2F, 0x00, "LDI  2F", 2);
    DIS(0x0000, 0xB3, 0x00, 0x00, "PHI  3", 1);
    DIS(0x0000, 0xDF, 0x00, 0x00, "SEP  F", 1);
    DIS(0x0000, 0x00, 0x00, 0x00, "IDL", 1);
    DIS(0x0000, 0x01, 0x00, 0x00, "LDN  1", 1);
    DIS(0x0000, 0x61, 0x00, 0x00, "OUT  1", 1);
    DIS(0x0000, 0x69, 0x00, 0x00, "INP  1", 1);
    DIS(0x0000, 0x68, 0x00, 0x00, "INP  0", 1);
    DIS(0x0000, 0x7B, 0x00, 0x00, "SEQ", 1);
    DIS(0x0000, 0xC4, 0x00, 0x00, "NOP", 1);
    DIS(0x0000, 0xFE, 0x00, 0x00, "SHL", 1);
    DIS(0x0000, 0x7C, 0x12, 0x00, "ADCI 12", 2);
    DIS(0x0000, 0xC0, 0x12, 0x34, "LBR  1234", 3);
    DIS(0x0000, 0xCA, 0xAB, 0xCD, "LBNZ ABCD", 3);
    DIS(0x0100, 0x30, 0x12, 0x00, "BR   0112", 2);
    DIS(0x01FF, 0x3A, 0x12, 0x00, "BNZ  0212", 2);     /* the operand is on page 02 */
    DIS(0x0000, 0x38, 0x55, 0x00, "SKP", 2);
    DIS(0x0000, 0x3C, 0x00, 0x00, "BN1  0000", 2);

    for (op = 0; op < 256; op++) {              /* every opcode has a name */
        int len;

        b[0] = (uint8_t)op;
        b[1] = b[2] = 0;
        len = dis1802(0, b, text, sizeof text);
        CHECK(len >= 1 && len <= 3 && text[0] >= 'A' && text[0] <= 'Z');
    }
#undef DIS
}

/* RESET turns the 1861's picture off (the chip is on the same CLEAR line), so a
 * program that is loaded and started begins with the display off. */
static void test_reset_turns_video_off(void)
{
    static elf_t m;
    static const uint8_t prog[] = { 0x69, 0x30, 0x01 };     /* INP 1, then wait */

    elf_init(&m);
    memcpy(m.ram, prog, sizeof prog);
    elf_set_switches(&m, true, false, false);
    elf_run(&m, 100);
    CHECK(m.video.on);
    elf_set_switches(&m, false, false, false);              /* RESET */
    elf_run(&m, 10);
    CHECK(!m.video.on && !m.video.int_active);
    elf_set_switches(&m, true, false, false);               /* run again */
    elf_run(&m, 100);
    CHECK(m.video.on);                                      /* the program turns it on */
}

/* The program the Elf holds at power-on, RANDTONE: three loops and the 1861.
 * The outer one makes a new number N (shown on the displays) with a chain of
 * ADC and ADD that visits all 256 values, and lights a block on the 1861 picture:
 * N's high nibble is a row and its low nibble a column of a 16 x 16 grid of
 * 4 x 8 pixel blocks. The picture in the RAM is 16 lines of 8 bytes (N / 2 is the
 * byte's offset) and the interrupt routine shows each line 8 times. The middle one toggles Q until R7's high byte is 0, the
 * inner one waits N turns between toggles and takes one off R7 each turn, so every
 * note lasts about the same time. Then the block is cleared and it starts again. */
#define BOOT_PICTURE    0x0400u     /* the picture in the RAM: 16 lines of 8 bytes */
#define BOOT_PICTURE_BYTES 128u
#define BOOT_NOTE_MIN   50000u      /* machine cycles in a note (with the DMA's share) */
#define BOOT_NOTE_MAX   70000u
#define BOOT_NOTE_MAX_SMALL_N 200000u   /* N below 8 or above F8: the toggling takes the time */
#define BOOT_IN_NOTE    100u        /* the note during which IN is pressed */

/* Is the picture in the RAM what we expect, and does the 1861 show it with every
 * line 8 times? */
static bool picture_is(const elf_t *m, const uint8_t *want)
{
    uint32_t i;
    bool ok = true;

    for (i = 0; i < 1024; i++)
        if (m->ram[BOOT_PICTURE + i] != (i < BOOT_PICTURE_BYTES ? want[i] : 0u))
            ok = false;
    for (i = 0; i < 128u * 8u; i++)
        if (m->video.bits[i / 8u][i % 8u] != want[(i / 64u) * 8u + i % 8u])
            ok = false;
    return ok;
}

/* Author: Thomas Dzubin */
static void test_boot_program(void)
{
    static elf_t m;
    static bool seen[256];
    uint8_t want[BOOT_PICTURE_BYTES];
    uint32_t n_distinct = 0;
    uint32_t i, k;

    memset(want, 0, sizeof want);
    elf_init(&m);
    elf_load_image(&m, BOOT_PROGRAM, BOOT_PROGRAM_SIZE);
    elf_set_switches(&m, true, false, false);

    /* the first note starts: run until the displays change from their start */
    for (i = 0; i < 100 && m.display == 0; i++)
        elf_run(&m, 4);
    CHECK(m.display != 0);
    CHECK(m.video.on);                               /* the program turned the 1861 on */

    /* then every note of the whole sequence; the blocks stay lit */
    for (i = 0; i < 256; i++) {
        uint32_t n = m.display;
        uint32_t t = 0;

        t += elf_run(&m, 20000);                     /* a few frames of the picture */
        while (m.video.line >= 70u)                  /* a whole picture is in, not part of one */
            t += elf_run(&m, 4);
        want[n / 2u] |= (n & 1u) ? 0x0Fu : 0xF0u;    /* this note's block, a nibble of a byte */
        CHECK(picture_is(&m, want));                 /* all the blocks so far, 8 lines tall */

        if (i == BOOT_IN_NOTE) {                     /* the IN button (the I key) clears it all */
            elf_in_button(&m, true);
            elf_run(&m, 12000);
            elf_in_button(&m, false);
            elf_run(&m, 8000);
            memset(want, 0, sizeof want);
            while (m.video.line >= 70u)
                elf_run(&m, 4);
            CHECK(picture_is(&m, want));
        }
        while (m.display == n && t < 400000u)
            t += elf_run(&m, 4);
        CHECK(m.display != n);
        if (i != BOOT_IN_NOTE)                       /* (a held IN button adds the clearing) */
            CHECK(t >= BOOT_NOTE_MIN &&
                  t <= (n < 8 || n > 0xF8 ? BOOT_NOTE_MAX_SMALL_N : BOOT_NOTE_MAX));
        if (!seen[n]) {
            seen[n] = true;
            n_distinct++;
        }
    }
    for (k = 0; k < BOOT_PICTURE_BYTES; k++)
        CHECK(want[k] == 0xFFu || want[k] == 0x0Fu || want[k] == 0xF0u || want[k] == 0u);
    CHECK(n_distinct == 256);                        /* every number once in 256 notes */
    CHECK(BOOT_PROGRAM[0] == 0x90 && BOOT_PROGRAM_SIZE == 256);

    /* the source shown in the editor assembles to exactly that program */
    CHECK(asm_assemble(&isa_1802, &assembler, 0, ELF_RAM_SIZE, BOOT_SOURCE, sizeof BOOT_SOURCE - 1) == 0);
    CHECK(memcmp(assembler.image, BOOT_PROGRAM, BOOT_PROGRAM_SIZE) == 0);
}

/* The small program in the README, "Programming the Elf": the keypad (INP 4)
 * copied to the displays (OUT 4).
 *
 * Author: Thomas Dzubin */
static void test_port4_example(void)
{
    static elf_t m;
    static const uint8_t prog[] = {
        0xF8, 0x00, 0xB2, 0xF8, 0x40, 0xA2, 0xE2,           /* R2 = 0040, X = 2   */
        0x6C, 0x64, 0x22, 0x30, 0x07                        /* INP 4, OUT 4, DEC 2, BR */
    };

    elf_init(&m);
    memcpy(m.ram, prog, sizeof prog);
    elf_set_switches(&m, true, false, false);
    elf_run(&m, 100);
    CHECK(m.display == 0x00);                               /* nothing typed yet */
    elf_key_hex(&m, 0x3);
    elf_key_hex(&m, 0xA);                                   /* 3 then A: the latch is 3A */
    elf_run(&m, 100);
    CHECK(m.display == 0x3A);
    CHECK(m.cpu.r[2] == 0x0040 || m.cpu.r[2] == 0x0041);    /* OUT adds one, the DEC takes it off */
    elf_key_hex(&m, 0x7);
    elf_run(&m, 100);
    CHECK(m.display == 0xA7);                               /* the older digit is the high one */
}

/* A program that wants the ASCII keyboard shows it: fetching B3 or BN3 (testing
 * EF3) or running INP 7 is counted, and nothing else is.
 *
 * Author: Thomas Dzubin */
static void test_keyboard_polls(void)
{
    static elf_t m;
    static const uint8_t quiet[] = { 0x7B, 0x7A, 0x30, 0x00 };      /* SEQ, REQ, BR 0 */
    static const uint8_t b3[]    = { 0x36, 0x00, 0x30, 0x00 };      /* B3 0, BR 0     */
    static const uint8_t bn3[]   = { 0x3E, 0x00, 0x30, 0x00 };      /* BN3 0, BR 0    */
    static const uint8_t inp7[]  = { 0xE2, 0x6F, 0x30, 0x01 };      /* SEX 2, INP 7, BR 1 */
    static const uint8_t b4[]    = { 0x37, 0x00, 0x30, 0x00 };      /* B4 (IN button), BR 0 */

    elf_init(&m);
    memcpy(m.ram, quiet, sizeof quiet);
    elf_set_switches(&m, true, false, false);
    elf_run(&m, 1000);
    CHECK(m.kbd_polls == 0);

    elf_init(&m);
    memcpy(m.ram, b3, sizeof b3);
    elf_set_switches(&m, true, false, false);
    elf_run(&m, 1000);
    CHECK(m.kbd_polls > 100);

    elf_init(&m);
    memcpy(m.ram, bn3, sizeof bn3);
    elf_set_switches(&m, true, false, false);
    elf_run(&m, 1000);
    CHECK(m.kbd_polls > 100);

    elf_init(&m);
    memcpy(m.ram, inp7, sizeof inp7);
    elf_set_switches(&m, true, false, false);
    elf_run(&m, 1000);
    CHECK(m.kbd_polls > 100);

    elf_init(&m);
    memcpy(m.ram, b4, sizeof b4);
    elf_set_switches(&m, true, false, false);
    elf_run(&m, 1000);
    CHECK(m.kbd_polls == 0);

    elf_init(&m);                                   /* the power-on program never asks */
    elf_load_image(&m, BOOT_PROGRAM, BOOT_PROGRAM_SIZE);
    elf_set_switches(&m, true, false, false);
    elf_run(&m, ELF_CYCLES_PER_SEC);
    CHECK(m.kbd_polls == 0);
}

/* The disassembly marks the targets of branches and jumps.
 *
 * Author: Thomas Dzubin */
static void test_branch_targets(void)
{
    static uint8_t image[ELF_RAM_SIZE];
    static uint8_t marks[ELF_RAM_SIZE / 8];
    static const uint8_t prog[] = {
        0xF8, 0x05,             /* 0000 LDI 05          */
        0xFF, 0x01,             /* 0002 SMI 01          */
        0x3A, 0x02,             /* 0004 BNZ 0002        */
        0x38, 0x00,             /* 0006 SKP (no target) */
        0xC8,                   /* 0008 LSKP (none)     */
        0xC0, 0x01, 0x23,       /* 0009 LBR 0123        */
    };
    uint8_t b[3] = { 0x3A, 0x02, 0x00 };
    int length;

    CHECK(dis1802_target(0x01FF, b, &length) == 0x0202 && length == 2);  /* the operand's page */
    b[0] = 0xC3; b[1] = 0x12; b[2] = 0x34;
    CHECK(dis1802_target(0x0000, b, &length) == 0x1234 && length == 3);
    b[0] = 0xF8;
    CHECK(dis1802_target(0x0000, b, &length) == -1 && length == 2);
    b[0] = 0xD3;
    CHECK(dis1802_target(0x0000, b, &length) == -1 && length == 1);

    memset(image, 0, sizeof image);
    memcpy(image, prog, sizeof prog);
    dis1802_mark_targets(image, sizeof image, marks);
    CHECK((marks[0x02 >> 3] >> (0x02 & 7)) & 1);                          /* the BNZ's */
    CHECK((marks[0x0123 >> 3] >> (0x0123 & 7)) & 1);                      /* the LBR's */
    CHECK(!((marks[0x06 >> 3] >> (0x06 & 7)) & 1));                       /* SKP has none */
    CHECK(!((marks[0x00 >> 3] >> (0x00 & 7)) & 1));
    CHECK(!((marks[0x08 >> 3] >> (0x08 & 7)) & 1));
}

/* A basic Elf has no 1861: EF1 stays high, there is no interrupt or DMA from it,
 * and INP 1 does nothing. (With the chip, EFX pulls EF1 low in the lines around
 * the picture.)
 *
 * Author: Thomas Dzubin */
static void test_video_option(void)
{
    static elf_t m;
    static const uint8_t prog[] = {
        0xF8, 0x00, 0xB2,       /* 0000 LDI 00, PHI 2                              */
        0xF8, 0x40, 0xA2,       /* 0003 LDI 40, PLO 2: R2 = 0040                   */
        0xE2,                   /* 0006 SEX 2 (INP stores its byte at M(R(X)))     */
        0x69,                   /* 0007 INP 1: the display on (if there is a chip) */
        0x34, 0x0E,             /* 0008 B1 000E                                    */
        0x30, 0x08,             /* 000A BR 0008                                    */
        0x00, 0x00,
        0x7B,                   /* 000E SEQ: EF1 was low                           */
        0x30, 0x0F              /* 000F BR 000F                                    */
    };

    elf_init(&m);
    CHECK(m.video_installed);                               /* the Elf II is the default */
    memcpy(m.ram, prog, sizeof prog);
    elf_set_switches(&m, true, false, false);
    elf_run(&m, 2u * 14u * 262u);                           /* two frames' worth of cycles */
    CHECK(m.video.on);
    CHECK(m.cpu.q);                                         /* B1 was taken: EFX went low */

    elf_init(&m);
    elf_set_video(&m, false);
    memcpy(m.ram, prog, sizeof prog);
    elf_set_switches(&m, true, false, false);
    elf_run(&m, 3u * 14u * 262u);
    CHECK(!m.video.on);                                     /* INP 1 did nothing */
    CHECK(!m.cpu.q);                                        /* EF1 stayed high */
    CHECK(m.video.frames == 0);                             /* the chip never ran */
    CHECK(m.cpu.p == 0 && m.cpu.r[1] == 0);                 /* no interrupt took the CPU */

    elf_set_video(&m, true);                                /* fitted again: EFX works */
    elf_run(&m, 3u * 14u * 262u);
    CHECK(m.cpu.q && !m.video.on);                          /* B1 taken; the display is still off */
}
