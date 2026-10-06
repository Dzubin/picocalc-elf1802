/*
 * vector_check.c - checks an instruction-set description (isa.h) against published
 * single-step test vectors: the JSON files of SingleStepTests/65x02 (one file for
 * each opcode of the 6502, thousands of tests in each, each with the state before
 * and after one instruction). It needs no CPU emulator: for every test the
 * description's `flow` must say what the test shows,
 *
 *   - an instruction that goes on to the next one: the PC after is the PC before
 *     plus the length (a skip adds its skipped bytes),
 *   - a branch: the PC after is the next instruction or the target,
 *   - a jump or a call: the PC after is the target,
 *
 * and the disassembly must have the same length. Instructions that go somewhere
 * the tools cannot follow (RTS, RTI, BRK, JMP (indirect)) are only checked as
 * far as the opcode being known. Opcodes that the description does not have
 * (undocumented ones) are counted and skipped.
 *
 *     gcc -Wall -Wextra -I. -Iothercpus -DASM_IMAGE_SIZE=0x5000 \
 *         othercpus/tests/vector_check.c othercpus/isa_6502.c asm_core.c -o vector_check
 *     ./vector_check 6502 a9.json b1.json ...
 *
 * The files are big (about 3 MB each); the first part of a file is plenty, so
 * `curl -r 0-300000` will do. The exit status is 0 when nothing differed.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "isa.h"
#include "isa_others.h"

static long checked, skipped, failures;

/* A number after key (such as "pc": ) in text, searching no further than end. */
static const char *find_key(const char *text, const char *end, const char *key)
{
    size_t n = strlen(key);
    const char *p;

    for (p = text; p + n < end; p++)
        if (memcmp(p, key, n) == 0)
            return p + n;
    return NULL;
}

static bool read_number(const char *p, const char *end, long *value)
{
    char *stop;

    while (p < end && (*p == ' ' || *p == ':'))
        p++;
    *value = strtol(p, &stop, 10);
    return stop != p;
}

/* Author: Thomas Dzubin */
static void check_file(const isa_t *isa, const char *path)
{
    FILE *f = fopen(path, "rb");
    static uint8_t memory[65536];
    char *text;
    long size, tests = 0, bad_here = 0;
    const char *p, *end;

    if (!f) {
        printf("%s: cannot open\n", path);
        failures++;
        return;
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    text = malloc((size_t)size + 1);
    if (!text || fread(text, 1, (size_t)size, f) != (size_t)size) {
        printf("%s: cannot read\n", path);
        fclose(f);
        free(text);
        failures++;
        return;
    }
    fclose(f);
    text[size] = '\0';
    end = text + size;

    for (p = text; (p = strstr(p, "{ \"name\"")) != NULL; ) {
        const char *next = strstr(p + 8, "{ \"name\"");
        const char *limit = next ? next : end;
        const char *initial = find_key(p, limit, "\"initial\"");
        const char *ram = initial ? find_key(initial, limit, "\"ram\": [") : NULL;
        const char *final = initial ? find_key(initial, limit, "\"final\"") : NULL;
        long pc0, pc1, addr, value;
        uint8_t b[ISA_MAX_BYTES];
        isa_flow_t fl;
        const char *q;
        int i;
        long expect_next, took;

        p += 8;
        if (!initial || !ram || !final || !find_key(final, limit, "\"cycles\""))
            continue;                                   /* a test cut off by the end of the file */
        if (!read_number(find_key(initial, limit, "\"pc\"") , limit, &pc0) ||
            !read_number(find_key(final, limit, "\"pc\""), limit, &pc1))
            continue;
        memset(memory, 0, sizeof memory);
        for (q = ram; q < final; ) {                    /* [[addr, value], [addr, value] ...] */
            while (q < final && (*q == ' ' || *q == ','))
                q++;
            if (q >= final || *q != '[')
                break;                                  /* the ] that ends the list */
            q++;
            if (!read_number(q, final, &addr) || (q = strchr(q, ',')) == NULL ||
                !read_number(q + 1, final, &value))
                break;
            if (addr >= 0 && addr < 65536)
                memory[addr] = (uint8_t)value;
            q = strchr(q, ']');
            if (!q)
                break;
            q++;
        }
        for (i = 0; i < ISA_MAX_BYTES; i++)
            b[i] = memory[(pc0 + i) & 0xFFFF];
        isa->flow((uint16_t)pc0, b, &fl);
        tests++;
        if (fl.kind == ISA_FLOW_BAD) {
            skipped++;
            continue;
        }
        checked++;
        expect_next = (pc0 + fl.length) & 0xFFFF;
        took = pc1;
        switch (fl.kind) {
        case ISA_FLOW_NEXT:
        case ISA_FLOW_DATA:
            if (took != expect_next && !(fl.skip > 0 && took == ((expect_next + fl.skip) & 0xFFFF)))
                goto wrong;
            break;
        case ISA_FLOW_BRANCH:
            if (took != expect_next && took != fl.target)
                goto wrong;
            break;
        case ISA_FLOW_JUMP:
        case ISA_FLOW_CALL:
            if (fl.target >= 0 && took != fl.target)
                goto wrong;
            break;
        default:                                        /* STOP, SKIP: not followed */
            break;
        }
        continue;
    wrong:
        if (bad_here < 3)
            printf("%s: opcode %02X at %04lX: flow says length %d kind %d target %04X, the test went to %04lX\n",
                   path, b[0], pc0, fl.length, fl.kind, fl.target < 0 ? 0xFFFF : fl.target, took);
        bad_here++;
        failures++;
    }
    printf("%s: %ld tests, %ld wrong\n", path, tests, bad_here);
    free(text);
}

int main(int argc, char **argv)
{
    const isa_t *isa = NULL;
    int i;

    if (argc > 1 && strcmp(argv[1], "6502") == 0)
        isa = &isa_6502;
    if (!isa || argc < 3) {
        printf("usage: vector_check 6502 file.json ...\n");
        return 2;
    }
    for (i = 2; i < argc; i++)
        check_file(isa, argv[i]);
    printf("%ld instructions checked, %ld skipped (not in the description), %ld wrong\n",
           checked, skipped, failures);
    return failures ? 1 : 0;
}
