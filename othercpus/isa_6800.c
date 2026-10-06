/*
 * isa_6800.c - the Motorola 6800's and 6803's instruction sets for the tools: how
 * to write an instruction, which way the program goes after it, and how to turn
 * assembly text into bytes (isa_6800_const.h has the tables). The 6803 is the
 * 6800 and a few more instructions.
 *
 * The source is written the usual way: LDAA #$12, LDAA $12, LDAA $12,X, LDAA
 * $1234, LDX #$1234, BNE label, with the accumulator in the mnemonic (LDAA,
 * LDAB, ASLA). An operand below 256 that is not a forward reference uses the
 * direct form where there is one; ".W" after the mnemonic (LDAA.W $12) forces
 * the extended form and ".B" the direct one.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "asm_core.h"
#include "isa.h"
#include "isa_others.h"
#include "isa_6800_const.h"

typedef struct {
    char    name[6];
    uint8_t mode;
    uint8_t level;                      /* the first processor with it            */
    bool    defined;
} entry_t;

static entry_t table[256];

static void set(unsigned opcode, const char *name, int mode, int level)
{
    strncpy(table[opcode].name, name, sizeof table[opcode].name - 1);
    table[opcode].mode = (uint8_t)mode;
    table[opcode].level = (uint8_t)level;
    table[opcode].defined = true;
}

/* Author: Thomas Dzubin */
static void build(void)
{
    static const struct { uint8_t opcode; const char *name; uint8_t mode, level; } ops[] = ISA6800_OPS;
    static const struct { uint8_t nibble; const char *name; } memory[] = ISA6800_MEMORY;
    static const struct { uint8_t nibble; const char *name; uint8_t imm; } accum[] = ISA6800_ACCUM;
    size_t i;
    int acc;

    if (table[0x01].defined)
        return;
    for (i = 0; i < sizeof ops / sizeof ops[0]; i++)
        set(ops[i].opcode, ops[i].name, ops[i].mode, ops[i].level);
    for (i = 0; i < sizeof memory / sizeof memory[0]; i++) {
        set(0x60u | memory[i].nibble, memory[i].name, M68_IDX, L6800);
        set(0x70u | memory[i].nibble, memory[i].name, M68_EXT, L6800);
    }
    for (i = 0; i < sizeof accum / sizeof accum[0]; i++) {
        for (acc = 0; acc < 2; acc++) {             /* A, then B */
            char name[6];
            unsigned top = acc ? 0xC0u : 0x80u;

            snprintf(name, sizeof name, "%s%c", accum[i].name, acc ? 'B' : 'A');
            if (accum[i].imm)
                set(top | accum[i].nibble, name, M68_IMM8, L6800);
            set((top + 0x10u) | accum[i].nibble, name, M68_DIR, L6800);
            set((top + 0x20u) | accum[i].nibble, name, M68_IDX, L6800);
            set((top + 0x30u) | accum[i].nibble, name, M68_EXT, L6800);
        }
    }
}

static const entry_t *by_opcode(uint8_t opcode, int level)
{
    build();
    return table[opcode].defined && table[opcode].level <= level ? &table[opcode] : NULL;
}

static const entry_t *by_name(const char *name, int mode, int level, unsigned *opcode)
{
    unsigned i;

    build();
    for (i = 0; i < 256; i++)
        if (table[i].defined && table[i].level <= level && table[i].mode == mode &&
            strcmp(table[i].name, name) == 0) {
            *opcode = i;
            return &table[i];
        }
    return NULL;
}

static bool known_name(const char *name, int level)
{
    unsigned i;

    build();
    for (i = 0; i < 256; i++)
        if (table[i].defined && table[i].level <= level && strcmp(table[i].name, name) == 0)
            return true;
    return false;
}

static int mode_length(int mode)
{
    switch (mode) {
    case M68_INH:
        return 1;
    case M68_IMM16:
    case M68_EXT:
        return 3;
    default:
        return 2;
    }
}

/* ------------------------------------------------------------------ */
/*  Disassembling                                                       */
/* ------------------------------------------------------------------ */
/* Author: Thomas Dzubin */
static int disasm_l(int level, uint16_t addr, const uint8_t *b, const char *target,
                    char *text, size_t size)
{
    const entry_t *e = by_opcode(b[0], level);
    unsigned w = ((unsigned)b[1] << 8) | b[2];            /* a 6800 word is high byte first */
    unsigned opcode;

    if (!e) {
        snprintf(text, size, "DB %02X", b[0]);
        return 1;
    }
    switch (e->mode) {
    case M68_INH:
        snprintf(text, size, "%s", e->name);
        break;
    case M68_IMM8:
        snprintf(text, size, "%-4s #$%02X", e->name, b[1]);
        break;
    case M68_IMM16:
        snprintf(text, size, "%-4s #$%04X", e->name, w);
        break;
    case M68_DIR:
        snprintf(text, size, "%-4s $%02X", e->name, b[1]);
        break;
    case M68_IDX:
        snprintf(text, size, "%-4s $%02X,X", e->name, b[1]);
        break;
    case M68_EXT:
        if (target && (strcmp(e->name, "JMP") == 0 || strcmp(e->name, "JSR") == 0))
            snprintf(text, size, "%-4s %s", e->name, target);
        else if (w < 0x100 && by_name(e->name, M68_DIR, level, &opcode) != NULL)
            snprintf(text, size, "%s.W $%04X", e->name, w);     /* a long form of a short address */
        else
            snprintf(text, size, "%-4s $%04X", e->name, w);
        break;
    default: {                                          /* M68_REL */
        unsigned dest = ((unsigned)addr + 2u + (unsigned)(int8_t)b[1]) & 0xFFFFu;

        if (target)
            snprintf(text, size, "%-4s %s", e->name, target);
        else
            snprintf(text, size, "%-4s $%04X", e->name, dest);
        break;
    }
    }
    return mode_length(e->mode);
}

/* Author: Thomas Dzubin */
static void flow_l(int level, uint16_t addr, const uint8_t *b, isa_flow_t *f)
{
    const entry_t *e = by_opcode(b[0], level);
    unsigned w = ((unsigned)b[1] << 8) | b[2];

    f->target = -1;
    f->skip = 0;
    f->kind = ISA_FLOW_NEXT;
    if (!e) {
        f->length = 1;
        f->kind = ISA_FLOW_BAD;
        return;
    }
    f->length = mode_length(e->mode);
    if (e->mode == M68_REL) {
        f->target = (int)(((unsigned)addr + 2u + (unsigned)(int8_t)b[1]) & 0xFFFFu);
        if (strcmp(e->name, "BRA") == 0)
            f->kind = ISA_FLOW_JUMP;
        else if (strcmp(e->name, "BSR") == 0)
            f->kind = ISA_FLOW_CALL;
        else if (strcmp(e->name, "BRN") != 0)
            f->kind = ISA_FLOW_BRANCH;
    } else if (strcmp(e->name, "JMP") == 0) {
        if (e->mode == M68_EXT) {
            f->kind = ISA_FLOW_JUMP;
            f->target = (int)w;
        } else {
            f->kind = ISA_FLOW_STOP;                    /* indexed: not known */
        }
    } else if (strcmp(e->name, "JSR") == 0) {
        f->kind = ISA_FLOW_CALL;
        if (e->mode == M68_EXT)
            f->target = (int)w;
        else if (e->mode == M68_DIR)
            f->target = b[1];
    } else if (strcmp(e->name, "RTS") == 0 || strcmp(e->name, "RTI") == 0 ||
               strcmp(e->name, "WAI") == 0 || strcmp(e->name, "SWI") == 0) {
        f->kind = ISA_FLOW_STOP;
    }
}

/* ------------------------------------------------------------------ */
/*  Assembling                                                          */
/* ------------------------------------------------------------------ */
/* Author: Thomas Dzubin */
static bool encode_l(int level, asm_t *a, const char *mnemonic, const char *operands)
{
    char name[6];
    char suffix = 0;
    const char *dot = strchr(mnemonic, '.');
    const char *p = asm_skip_space(operands);
    size_t n = dot ? (size_t)(dot - mnemonic) : strlen(mnemonic);
    unsigned opcode = 0;
    uint32_t v = 0;
    uint8_t byte;

    if (n == 0 || n >= sizeof name)
        return false;
    memcpy(name, mnemonic, n);
    name[n] = '\0';
    if (!known_name(name, level))
        return false;
    if (dot) {
        suffix = dot[1];
        if ((suffix != 'W' && suffix != 'B') || dot[2] != '\0') {
            asm_error(a, ASM_ERR_MNEMONIC);
            return true;
        }
    }

    if (*p == '\0') {                                   /* inherent */
        if (by_name(name, M68_INH, level, &opcode)) {
            asm_emit(a, (uint8_t)opcode);
        } else {
            asm_error(a, ASM_ERR_MISSING);
        }
        return true;
    }
    if (*p == '#') {                                    /* immediate */
        p++;
        if (by_name(name, M68_IMM8, level, &opcode)) {
            if (asm_expression(a, &p, &v) && asm_at_end(a, p) && asm_byte_value(a, v, &byte)) {
                asm_emit(a, (uint8_t)opcode);
                asm_emit(a, byte);
            }
        } else if (by_name(name, M68_IMM16, level, &opcode)) {
            if (asm_expression(a, &p, &v) && asm_at_end(a, p)) {
                asm_emit(a, (uint8_t)opcode);
                asm_emit(a, (uint8_t)(v >> 8));
                asm_emit(a, (uint8_t)(v & 0xFFu));
            }
        } else {
            asm_error(a, ASM_ERR_MODE);
        }
        return true;
    }
    {
        bool indexed = false;
        const char *q;

        if (*p == ',') {                                /* ,X: an offset of 0 */
            v = 0;
        } else if (!asm_expression(a, &p, &v)) {
            return true;
        }
        q = asm_skip_space(p);
        if (*q == ',') {
            q = asm_skip_space(q + 1);
            if (asm_upper(*q) != 'X' || asm_is_name_char(q[1])) {
                asm_error(a, ASM_ERR_MODE);
                return true;
            }
            p = q + 1;
            indexed = true;
        }
        if (!asm_at_end(a, p))
            return true;
        if (indexed) {
            if (!by_name(name, M68_IDX, level, &opcode)) {
                asm_error(a, ASM_ERR_MODE);
            } else if (asm_byte_value(a, v, &byte)) {
                asm_emit(a, (uint8_t)opcode);
                asm_emit(a, byte);
            }
            return true;
        }
        if (by_name(name, M68_REL, level, &opcode)) {   /* a branch to an address */
            long offset = (long)v - (long)(asm_location(a) + 2u);

            if (asm_pass(a) == 2 && (offset < -128 || offset > 127)) {
                asm_error(a, ASM_ERR_RANGE);
                return true;
            }
            asm_emit(a, (uint8_t)opcode);
            asm_emit(a, (uint8_t)(offset & 0xFF));
            return true;
        }
        {
            unsigned dir, ext;
            bool have_dir = by_name(name, M68_DIR, level, &dir) != NULL;
            bool have_ext = by_name(name, M68_EXT, level, &ext) != NULL;
            bool use_dir = have_dir && suffix != 'W' && v < 0x100u &&
                           (suffix == 'B' || !asm_forward(a) || !have_ext);

            if (suffix == 'B' && (!have_dir || v >= 0x100u)) {
                asm_error(a, ASM_ERR_MODE);
                return true;
            }
            if (use_dir) {
                asm_emit(a, (uint8_t)dir);
                asm_emit(a, (uint8_t)v);
            } else if (have_ext) {
                asm_emit(a, (uint8_t)ext);
                asm_emit(a, (uint8_t)(v >> 8));
                asm_emit(a, (uint8_t)(v & 0xFFu));
            } else {
                asm_error(a, ASM_ERR_MODE);
            }
        }
    }
    return true;
}

static bool reserved(const char *name)
{
    return (name[0] == 'A' || name[0] == 'B' || name[0] == 'X') && name[1] == '\0';
}

static int disasm_6800(uint16_t a, const uint8_t *b, const char *t, char *s, size_t n)
{
    return disasm_l(L6800, a, b, t, s, n);
}
static int disasm_6803(uint16_t a, const uint8_t *b, const char *t, char *s, size_t n)
{
    return disasm_l(L6803, a, b, t, s, n);
}
static void flow_6800(uint16_t a, const uint8_t *b, isa_flow_t *f) { flow_l(L6800, a, b, f); }
static void flow_6803(uint16_t a, const uint8_t *b, isa_flow_t *f) { flow_l(L6803, a, b, f); }
static bool encode_6800(asm_t *a, const char *m, const char *o) { return encode_l(L6800, a, m, o); }
static bool encode_6803(asm_t *a, const char *m, const char *o) { return encode_l(L6803, a, m, o); }

const isa_t isa_6800 = {
    "6800",
    3,
    true,                                                   /* DW: high byte first */
    ISA_NUM_DOLLAR_HEX | ISA_NUM_PERCENT_BINARY | ISA_NUM_0X,
    disasm_6800,
    flow_6800,
    NULL,
    encode_6800,
    reserved,
    NULL,
    NULL
};

const isa_t isa_6803 = {
    "6803",
    3,
    true,
    ISA_NUM_DOLLAR_HEX | ISA_NUM_PERCENT_BINARY | ISA_NUM_0X,
    disasm_6803,
    flow_6803,
    NULL,
    encode_6803,
    reserved,
    NULL,
    NULL
};
