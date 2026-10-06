/*
 * test_isa_others.c - checks of the instruction-set descriptions of the processors
 * other than the 1802 (the 6502, 8080, Z80, 6800, 6803 and 6809) and the tools that
 * use them: that every opcode reads back from its own disassembly to the same
 * bytes, that known programs assemble to known bytes, that the assembler's errors
 * are the right ones, and that the source made from any bytes assembles back to
 * them. Plain C, builds with any compiler, no hardware. From the project folder:
 *
 *     gcc -Wall -Wextra -I. -Iothercpus -DASM_IMAGE_SIZE=0x5000 \
 *         othercpus/tests/test_isa_others.c asm_core.c source_gen.c \
 *         othercpus/isa_6502.c othercpus/isa_8080.c othercpus/isa_z80.c \
 *         othercpus/isa_6800.c othercpus/isa_6809.c -o test_isa_others
 *     ./test_isa_others
 *
 * It prints one line per failed check and a summary, and its exit status is 0
 * when everything passed.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "asm_core.h"
#include "isa.h"
#include "isa_others.h"
#include "source_gen.h"

static int checks, failures;

#define CHECK(cond) do { \
        checks++; \
        if (!(cond)) { failures++; printf("FAIL line %d: %s\n", __LINE__, #cond); } \
    } while (0)

static asm_t assembler;

static int assemble(const isa_t *isa, const char *text)
{
    return asm_assemble(isa, &assembler, 0, ASM_IMAGE_SIZE, text, strlen(text));
}

/* Assemble text and say whether it made exactly the bytes want, from address at. */
static bool makes(const isa_t *isa, const char *text, unsigned at, const uint8_t *want, size_t n)
{
    return assemble(isa, text) == 0 && assembler.low == (int)at &&
           (size_t)assembler.bytes == n && memcmp(assembler.image + at, want, n) == 0;
}

/* Every opcode, with several operand bytes, must read back from its disassembly.
 *
 * Author: Thomas Dzubin */
static void round_trip(const isa_t *isa, int prefix)
{
    static const uint8_t fill[][4] = {
        { 0x12, 0x34, 0x56, 0x78 }, { 0x00, 0x00, 0x00, 0x00 }, { 0xFF, 0xFF, 0xFF, 0xFF },
        { 0x7F, 0x80, 0x01, 0xFE }, { 0x80, 0x7F, 0xFE, 0x01 }
    };
    int op;
    size_t f;
    int defined = 0;

    for (op = 0; op < 256; op++) {
        for (f = 0; f < sizeof fill / sizeof fill[0]; f++) {
            uint8_t b[ISA_MAX_BYTES];
            isa_flow_t fl;
            char text[160];
            char source[200];
            char num[12];
            int len, k;
            int i = 0;

            memset(b, 0, sizeof b);
            if (prefix)
                b[i++] = (uint8_t)prefix;
            b[i++] = (uint8_t)op;
            for (k = 0; k < 4 && i < ISA_MAX_BYTES; k++)
                b[i++] = fill[f][k];
            isa->flow(0x1000, b, &fl);
            if (fl.kind == ISA_FLOW_BAD || fl.kind == ISA_FLOW_DATA)
                continue;
            len = isa->disasm(0x1000, b, NULL, text, sizeof text);
            CHECK(len == fl.length);
            if (f == 0)
                defined++;
            srcgen_hex(isa, num, sizeof num, 0x1000, 4);
            snprintf(source, sizeof source, "ORG %s\n%s\n", num, text);
            if (!(assemble(isa, source) == 0 && assembler.bytes == (uint32_t)len &&
                  memcmp(assembler.image + 0x1000, b, (size_t)len) == 0)) {
                CHECK(false);
                printf("   %s %02X.%02X [%d]: \"%s\" made %u bytes, errors %d (%s)\n", isa->name,
                       prefix, op, (int)f, text, (unsigned)assembler.bytes, assembler.error_count,
                       assembler.error_count ? assembler.errors[0].text : "");
            }
        }
    }
    CHECK(defined > 0);
}

/* The source of random bytes assembles back to them, whole or cut off.
 *
 * Author: Thomas Dzubin */
static void source_round_trip(const isa_t *isa)
{
    static uint8_t image[4096];
    static char text[40000];
    static const size_t caps[] = { 120, 700, 5000, 40000 };
    uint32_t seed = 12345u;
    uint32_t done = 0;
    size_t i, c;

    for (i = 0; i < 3000; i++) {
        seed = seed * 1103515245u + 12345u;
        image[i] = (uint8_t)(seed >> 16);
    }
    for (c = 0; c < sizeof caps / sizeof caps[0]; c++) {
        size_t n = srcgen_from_image(isa, image, sizeof image, text, caps[c], &done);

        CHECK(n > 0 && n < caps[c] && done > 0);
        if (!(assemble(isa, text) == 0 && memcmp(assembler.image, image, done) == 0)) {
            CHECK(false);
            printf("   %s with room for %u characters: %d errors (%s)\n", isa->name,
                   (unsigned)caps[c], assembler.error_count,
                   assembler.error_count ? assembler.errors[0].text : "");
        }
    }
}

/* The Z80's DD CB d op and FD CB d op group has its opcode after the displacement,
 * so the round trip above (which puts the opcode second) does not reach it.
 *
 * Author: Thomas Dzubin */
static void z80_indexed_bit_group(void)
{
    static const uint8_t displacements[] = { 0x00, 0x7F, 0x80, 0x12 };
    int prefix, op, d, valid = 0;

    for (prefix = 0xDD; prefix <= 0xFD; prefix += 0x20) {
        for (d = 0; d < (int)sizeof displacements; d++) {
            for (op = 0; op < 256; op++) {
                uint8_t b[ISA_MAX_BYTES] = { (uint8_t)prefix, 0xCB, displacements[d], (uint8_t)op, 0 };
                isa_flow_t fl;
                char text[80], source[120];

                isa_z80.flow(0x1000, b, &fl);
                if (fl.kind == ISA_FLOW_BAD)
                    continue;
                if (d == 0 && prefix == 0xDD)
                    valid++;
                CHECK(fl.length == 4 && isa_z80.disasm(0x1000, b, NULL, text, sizeof text) == 4);
                snprintf(source, sizeof source, "ORG 1000H\n%s\n", text);
                CHECK(assemble(&isa_z80, source) == 0 && assembler.bytes == 4 &&
                      memcmp(assembler.image + 0x1000, b, 4) == 0);
            }
        }
    }
    CHECK(valid == 31);                 /* seven rotates and shifts, and 8 each of BIT, RES, SET */
}

/* Author: Thomas Dzubin */
int main(void)
{
    static const isa_t *const all[] = { &isa_6502, &isa_8080, &isa_z80, &isa_6800, &isa_6803,
                                        &isa_6809 };
    size_t k;

    for (k = 0; k < sizeof all / sizeof all[0]; k++) {
        round_trip(all[k], 0);
        source_round_trip(all[k]);
    }
    round_trip(&isa_6809, 0x10);                       /* the 6809's two pages of prefixed opcodes */
    round_trip(&isa_6809, 0x11);
    round_trip(&isa_z80, 0xCB);                        /* the Z80's four prefix groups */
    round_trip(&isa_z80, 0xED);
    round_trip(&isa_z80, 0xDD);
    round_trip(&isa_z80, 0xFD);
    z80_indexed_bit_group();

    /* ---- the 6502 ---- */
    {
        static const uint8_t prog[] = { 0xA9, 0x01, 0x8D, 0x00, 0x02, 0xA2, 0xFF, 0xCA, 0xD0, 0xFD,
                                        0x4C, 0x07, 0x06 };
        static const uint8_t modes[] = { 0xA5, 0x12, 0xB5, 0x12, 0xAD, 0x34, 0x12, 0xBD, 0x34, 0x12,
                                         0xB9, 0x34, 0x12, 0xA1, 0x12, 0xB1, 0x12, 0x6C, 0x34, 0x12,
                                         0x0A, 0x2A, 0xB6, 0x12, 0xAD, 0x12, 0x00 };

        CHECK(makes(&isa_6502, "ORG $0600\n LDA #$01\n STA $0200\n LDX #$FF\nloop: DEX\n BNE loop\n JMP loop\n",
                    0x600, prog, sizeof prog));
        CHECK(makes(&isa_6502,
                    "ORG $0400\n LDA $12\n LDA $12,X\n LDA $1234\n LDA $1234,X\n LDA $1234,Y\n LDA ($12,X)\n"
                    " LDA ($12),Y\n JMP ($1234)\n ASL A\n ROL\n LDX $12,Y\n LDA.W $12\n",
                    0x400, modes, sizeof modes));
        CHECK(assemble(&isa_6502, "ORG $0200\n BNE far\n DS 200\nfar: NOP\n") == 1);    /* too far */
        CHECK(assemble(&isa_6502, "LDA ($12,Y)\n") == 1);
        CHECK(assemble(&isa_6502, "STA #$12\n") == 1);
        CHECK(assemble(&isa_6502, "ORG $0200\n LDA later\nlater: NOP\n") == 0 &&
              assembler.image[0x200] == 0xAD);        /* a forward reference is an absolute address */
        CHECK(assemble(&isa_6502, "A: NOP\n") == 1);      /* A is the accumulator */
    }
    /* ---- the 8080 ---- */
    {
        static const uint8_t prog[] = { 0x3E, 0x05, 0x21, 0x00, 0x20, 0x77, 0x3D, 0xC2, 0x05, 0x01, 0x76 };
        static const uint8_t misc[] = { 0x78, 0xC5, 0xF1, 0x0A, 0x32, 0x34, 0x12, 0xCF, 0xD3, 0x10,
                                        0xFE, 0xFF, 0xE9, 0x09, 0x1B };

        CHECK(makes(&isa_8080, "ORG 100H\n MVI A,5\n LXI H,2000H\nloop: MOV M,A\n DCR A\n JNZ loop\n HLT\n",
                    0x100, prog, sizeof prog));
        CHECK(makes(&isa_8080, "ORG 100H\n MOV A,B\n PUSH B\n POP PSW\n LDAX B\n STA 1234H\n RST 1\n OUT 10H\n"
                    " CPI 0FFH\n PCHL\n DAD B\n DCX D\n", 0x100, misc, sizeof misc));
        CHECK(assemble(&isa_8080, "MOV M,M\n") == 1);
        CHECK(assemble(&isa_8080, "MVI A\n") == 1);
        CHECK(assemble(&isa_8080, "RST 8\n") == 1);
        CHECK(assemble(&isa_8080, "H: NOP\n") == 1);        /* H is a register */
        CHECK(assemble(&isa_8080, "DW 1234H\n") == 0 && assembler.image[0] == 0x34);   /* low byte first */
    }
    /* ---- the Z80 ---- */
    {
        static const uint8_t prog[] = { 0x3E, 0x05, 0x21, 0x00, 0x20, 0x77, 0x3D, 0x20, 0xFC, 0x76 };
        static const uint8_t modes[] = {
            0xDD, 0x7E, 0x05, 0xFD, 0x70, 0xFE, 0xDD, 0x36, 0x01, 0x42, 0xDD, 0xCB, 0x07, 0x5E,
            0xFD, 0xCB, 0x00, 0xC6, 0x22, 0x34, 0x12, 0xED, 0x43, 0x34, 0x12, 0xED, 0xB0, 0x08,
            0xED, 0x78, 0xED, 0x41, 0xED, 0x5E, 0xFF, 0xDD, 0xE9, 0x19, 0xED, 0x4A, 0xCB, 0x00,
            0xC4, 0x00, 0x10, 0xF9, 0xD9, 0xED, 0x44, 0x80, 0xD6, 0x05, 0xBE, 0xD3, 0x10, 0xC8,
            0xED, 0x57, 0xDD, 0xE5 };
        static const uint8_t tail[] = { 0x08, 0x3A, 0x34, 0x12, 0x7E, 0xC3, 0x00, 0x20, 0x18, 0xFE };

        CHECK(makes(&isa_z80, "ORG 100H\n LD A,5\n LD HL,2000H\nloop: LD (HL),A\n DEC A\n JR NZ,loop\n HALT\n",
                    0x100, prog, sizeof prog));
        CHECK(makes(&isa_z80,
                    "ORG 100H\n LD A,(IX+5)\n LD (IY-2),B\n LD (IX+1),42H\n BIT 3,(IX+7)\n SET 0,(IY)\n"
                    " LD (1234H),HL\n LD (1234H),BC\n LDIR\n EX AF,AF'\n IN A,(C)\n OUT (C),B\n IM 2\n"
                    " RST 38H\n JP (IX)\n ADD HL,DE\n ADC HL,BC\n RLC B\n CALL NZ,1000H\n LD SP,HL\n"
                    " EXX\n NEG\n ADD A,B\n SUB 5\n CP (HL)\n OUT (10H),A\n RET Z\n LD A,I\n PUSH IX\n",
                    0x100, modes, sizeof modes));
        /* a comment after AF', lower case, a character and a label for the jump */
        CHECK(makes(&isa_z80, "ORG 100H\n ex af,af' ; swap\n ld a,(1234h)\n ld a,(hl)\n jp 2000h\nhere: jr here\n",
                    0x100, tail, sizeof tail));
        CHECK(assemble(&isa_z80, "LD A,B,C\n") == 1);
        CHECK(assemble(&isa_z80, "LD (HL),(HL)\n") == 1);          /* that is HALT */
        CHECK(assemble(&isa_z80, "FROB A\n") == 1);
        CHECK(assemble(&isa_z80, "LD A,(IX+200)\n") == 1);        /* the displacement is a signed byte */
        CHECK(assemble(&isa_z80, "ORG 100H\n JR far\n DS 200\nfar: NOP\n") == 1);
        CHECK(assemble(&isa_z80, "SLL B\n") == 1);                /* undocumented */
        CHECK(assemble(&isa_z80, "RST 9\n") == 1);
        CHECK(assemble(&isa_z80, "HL: NOP\n") == 1);              /* HL is a register */
        CHECK(assemble(&isa_z80, "DW 1234H\n") == 0 && assembler.image[0] == 0x34);   /* low byte first */
        /* a forward label gives the same sizes in both passes */
        CHECK(assemble(&isa_z80, "ORG 100H\n LD HL,later\n LD A,(later)\n JR later\nlater: NOP\n") == 0 &&
              assembler.bytes == 3 + 3 + 2 + 1);
    }
    /* ---- the 6800 and the 6803 ---- */
    {
        static const uint8_t prog[] = { 0x86, 0x05, 0x97, 0x20, 0x4A, 0x26, 0xFD, 0x39 };
        static const uint8_t d[] = { 0xCC, 0x12, 0x34, 0xC3, 0x00, 0x01, 0x3D, 0x3A };
        static const uint8_t modes[] = { 0x96, 0x12, 0xA6, 0x05, 0xB6, 0x12, 0x34, 0xCE, 0x12, 0x34,
                                         0xBD, 0x12, 0x34, 0xB6, 0x00, 0x12 };

        CHECK(makes(&isa_6800, "ORG $0100\n LDAA #$05\n STAA $20\nloop: DECA\n BNE loop\n RTS\n",
                    0x100, prog, sizeof prog));
        CHECK(makes(&isa_6803, "ORG $0100\n LDD #$1234\n ADDD #1\n MUL\n ABX\n", 0x100, d, sizeof d));
        CHECK(makes(&isa_6800, "ORG $0100\n LDAA $12\n LDAA 5,X\n LDAA $1234\n LDX #$1234\n JSR $1234\n"
                    " LDAA.W $12\n", 0x100, modes, sizeof modes));
        CHECK(assemble(&isa_6800, "LDD #1\n") == 1);          /* the 6803 has it, the 6800 does not */
        CHECK(assemble(&isa_6803, "LDD #1\n") == 0);
        CHECK(assemble(&isa_6800, "STAA #5\n") == 1);
        CHECK(assemble(&isa_6800, "DW 1234\n") == 0 && assembler.image[0] == 0x04);     /* high byte first */
    }
    /* ---- the 6809 ---- */
    {
        static const uint8_t prog[] = { 0x86, 0x12, 0x8E, 0x10, 0x00, 0xA6, 0x80, 0x5A, 0x26, 0xFB,
                                        0x39, 0x30, 0x05, 0x1F, 0x89, 0x34, 0x16 };
        static const uint8_t want[] = { 0xA6, 0x84, 0xA6, 0xA0, 0xA6, 0xC2, 0xA6, 0x61, 0xA6, 0x85,
                                        0xA6, 0x8B, 0xA6, 0x88, 0x08, 0xA6, 0x89, 0x00, 0x01, 0xA6,
                                        0x9F, 0x12, 0x34, 0xA6, 0x98, 0x05, 0xA6, 0xD4, 0xA6, 0xF1 };

        CHECK(makes(&isa_6809, "ORG $0200\n LDA #$12\n LDX #$1000\nloop: LDA ,X+\n DECB\n BNE loop\n RTS\n"
                    " LEAX 5,X\n TFR A,B\n PSHS A,B,X\n", 0x200, prog, sizeof prog));
        CHECK(makes(&isa_6809,
                    "ORG $0300\n LDA ,X\n LDA ,Y+\n LDA ,-U\n LDA 1,S\n LDA B,X\n LDA D,X\n LDA <8,X\n"
                    " LDA >1,X\n LDA [$1234]\n LDA [5,X]\n LDA [,U]\n LDA [,S++]\n",
                    0x300, want, sizeof want));
        CHECK(assemble(&isa_6809, "ORG $0300\n LDA ,X+\n LBRA far\n BRA near\nnear: NOP\nfar: NOP\n") == 0);
        CHECK(assemble(&isa_6809, "LDA [,X+]\n") == 1);        /* ,R+ cannot be indirect */
        CHECK(assemble(&isa_6809, "TFR A,X\n") == 1);          /* 8-bit with 16-bit */
        CHECK(assemble(&isa_6809, "PSHS Q\n") == 1);
        /* LDA near,PCR: the offset counts from the instruction after this one, and a label
         * that is not known yet gets the long form */
        CHECK(assemble(&isa_6809, "ORG $0300\n LDA near,PCR\nnear: NOP\n") == 0 &&
              assembler.image[0x300] == 0xA6 && assembler.image[0x301] == 0x8D &&
              assembler.image[0x302] == 0x00 && assembler.image[0x303] == 0x00);   /* a forward label: 16 bits */
        CHECK(assemble(&isa_6809, "DW 1234\n") == 0 && assembler.image[0] == 0x04);   /* high byte first */
        CHECK(assemble(&isa_6809, "ORG $0300\n BRA far\n DS 200\nfar: NOP\n") == 1);
    }

    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
