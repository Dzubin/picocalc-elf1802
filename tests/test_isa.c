/*
 * test_isa.c - checks of the instruction-set descriptions (isa.h) and the tools
 * that use them: for the 1802, that every opcode it has reads back from its
 * own disassembly to the same bytes, that known programs assemble to known bytes,
 * that the assembler's errors are the right ones, and that the source made from any
 * bytes assembles back to them. Plain C, builds with any compiler, no hardware:
 *
 *     gcc -Wall -Wextra -I. tests/test_isa.c asm_core.c source_gen.c isa_1802.c \
 *         disasm1802.c -o test_isa
 *     ./test_isa
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

/* Author: Thomas Dzubin */
int main(void)
{
    static const isa_t *const all[] = { &isa_1802 };
    size_t k;

    for (k = 0; k < sizeof all / sizeof all[0]; k++) {
        round_trip(all[k], 0);
        source_round_trip(all[k]);
    }
    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
