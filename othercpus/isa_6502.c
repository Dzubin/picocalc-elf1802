/*
 * isa_6502.c - the MOS 6502's instruction set for the tools: how to write an
 * instruction, which way the program goes after it, and how to turn assembly
 * text into bytes. Documented opcodes only (isa_6502_const.h).
 *
 * The source is written the usual way: LDA #$12, LDA $12, LDA $12,X, LDA $1234,
 * LDA ($12,X), LDA ($12),Y, JMP ($1234), ASL A, BNE label. An operand below 256
 * that is not a forward reference uses the zero page form where there is one; a
 * ".W" after the mnemonic (LDA.W $12) forces the absolute form and ".B" the zero
 * page one, and the listing writes ".W" where a program really uses a long form
 * for a short address.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "asm_core.h"
#include "isa.h"
#include "isa_others.h"
#include "isa_6502_const.h"

typedef struct {
    uint8_t     opcode;
    const char *name;
    uint8_t     mode;
} op_t;

static const op_t ops[] = ISA6502_OPCODES;
#define OP_COUNT (sizeof ops / sizeof ops[0])

/* The table entry for an opcode, or NULL for one that is not documented. */
static const op_t *by_opcode(uint8_t opcode)
{
    static const op_t *index[256];
    static bool built;

    if (!built) {
        size_t i;

        for (i = 0; i < OP_COUNT; i++)
            index[ops[i].opcode] = &ops[i];
        built = true;
    }
    return index[opcode];
}

static const op_t *by_name(const char *name, int mode)
{
    size_t i;

    for (i = 0; i < OP_COUNT; i++)
        if (ops[i].mode == mode && strcmp(ops[i].name, name) == 0)
            return &ops[i];
    return NULL;
}

static bool known_name(const char *name)
{
    size_t i;

    for (i = 0; i < OP_COUNT; i++)
        if (strcmp(ops[i].name, name) == 0)
            return true;
    return false;
}

static int mode_length(int mode)
{
    switch (mode) {
    case M65_IMP:
    case M65_ACC:
        return 1;
    case M65_ABS:
    case M65_ABX:
    case M65_ABY:
    case M65_IND:
        return 3;
    default:
        return 2;
    }
}

/* ------------------------------------------------------------------ */
/*  Disassembling                                                       */
/* ------------------------------------------------------------------ */
/* The zero page form of an absolute mode, or -1: used to see whether a
 * long form of a short address needs a ".W" to read back the same. */
static int short_mode(int mode)
{
    return mode == M65_ABS ? M65_ZP : mode == M65_ABX ? M65_ZPX : mode == M65_ABY ? M65_ZPY : -1;
}

/* Author: Thomas Dzubin */
static int disasm(uint16_t addr, const uint8_t *b, const char *target, char *text, size_t size)
{
    const op_t *o = by_opcode(b[0]);
    unsigned w = (unsigned)b[1] | ((unsigned)b[2] << 8);
    char name[8];

    if (!o) {
        snprintf(text, size, "DB %02X", b[0]);
        return 1;
    }
    snprintf(name, sizeof name, "%s", o->name);
    switch (o->mode) {
    case M65_IMP:
        snprintf(text, size, "%s", name);
        break;
    case M65_ACC:
        snprintf(text, size, "%-4s A", name);
        break;
    case M65_IMM:
        snprintf(text, size, "%-4s #$%02X", name, b[1]);
        break;
    case M65_ZP:
        snprintf(text, size, "%-4s $%02X", name, b[1]);
        break;
    case M65_ZPX:
        snprintf(text, size, "%-4s $%02X,X", name, b[1]);
        break;
    case M65_ZPY:
        snprintf(text, size, "%-4s $%02X,Y", name, b[1]);
        break;
    case M65_ABS:
    case M65_ABX:
    case M65_ABY: {
        const char *index = o->mode == M65_ABX ? ",X" : (o->mode == M65_ABY ? ",Y" : "");
        bool jump = strcmp(name, "JMP") == 0 || strcmp(name, "JSR") == 0;

        if (jump && target) {
            snprintf(text, size, "%-4s %s", name, target);
        } else if (w < 0x100 && by_name(name, short_mode(o->mode)) != NULL) {
            snprintf(text, size, "%s.W $%04X%s", name, w, index);   /* a long form of a short address */
        } else {
            snprintf(text, size, "%-4s $%04X%s", name, w, index);
        }
        break;
    }
    case M65_IND:
        snprintf(text, size, "%-4s ($%04X)", name, w);
        break;
    case M65_IZX:
        snprintf(text, size, "%-4s ($%02X,X)", name, b[1]);
        break;
    case M65_IZY:
        snprintf(text, size, "%-4s ($%02X),Y", name, b[1]);
        break;
    default: {                                          /* M65_REL */
        unsigned dest = ((unsigned)addr + 2u + (unsigned)(int8_t)b[1]) & 0xFFFFu;

        if (target)
            snprintf(text, size, "%-4s %s", name, target);
        else
            snprintf(text, size, "%-4s $%04X", name, dest);
        break;
    }
    }
    return mode_length(o->mode);
}

/* Author: Thomas Dzubin */
static void flow(uint16_t addr, const uint8_t *b, isa_flow_t *f)
{
    const op_t *o = by_opcode(b[0]);

    f->target = -1;
    f->skip = 0;
    f->kind = ISA_FLOW_NEXT;
    if (!o) {
        f->length = 1;
        f->kind = ISA_FLOW_BAD;
        return;
    }
    f->length = mode_length(o->mode);
    if (o->mode == M65_REL) {
        f->kind = ISA_FLOW_BRANCH;
        f->target = (int)(((unsigned)addr + 2u + (unsigned)(int8_t)b[1]) & 0xFFFFu);
    } else if (strcmp(o->name, "JMP") == 0 && o->mode == M65_ABS) {
        f->kind = ISA_FLOW_JUMP;
        f->target = (int)(b[1] | ((unsigned)b[2] << 8));
    } else if (strcmp(o->name, "JSR") == 0) {
        f->kind = ISA_FLOW_CALL;
        f->target = (int)(b[1] | ((unsigned)b[2] << 8));
    } else if (strcmp(o->name, "JMP") == 0 || strcmp(o->name, "RTS") == 0 ||
               strcmp(o->name, "RTI") == 0 || strcmp(o->name, "BRK") == 0) {
        f->kind = ISA_FLOW_STOP;                        /* an indirect jump, a return */
    }
}

/* ------------------------------------------------------------------ */
/*  Assembling                                                          */
/* ------------------------------------------------------------------ */
/* Look for ",X" or ",Y" at *p (after spaces); returns 0, 'X' or 'Y' and moves p
 * past it. */
static char index_register(const char **p)
{
    const char *q = asm_skip_space(*p);
    char r;

    if (*q != ',')
        return 0;
    q = asm_skip_space(q + 1);
    r = asm_upper(*q);
    if ((r != 'X' && r != 'Y') || asm_is_name_char(q[1]))
        return 0;
    *p = q + 1;
    return r;
}

/* Author: Thomas Dzubin */
static bool encode(asm_t *a, const char *mnemonic, const char *operands)
{
    char name[8];
    char suffix = 0;
    const char *dot = strchr(mnemonic, '.');
    const char *p = asm_skip_space(operands);
    const op_t *o = NULL;
    uint32_t v = 0;
    int base = -1;                          /* the addressing mode before zero page is chosen */
    size_t n = dot ? (size_t)(dot - mnemonic) : strlen(mnemonic);

    if (n == 0 || n >= sizeof name)
        return false;
    memcpy(name, mnemonic, n);
    name[n] = '\0';
    if (!known_name(name))
        return false;
    if (dot) {
        suffix = dot[1];
        if ((suffix != 'W' && suffix != 'B') || dot[2] != '\0') {
            asm_error(a, ASM_ERR_MNEMONIC);
            return true;
        }
    }

    if (*p == '\0') {                                   /* implied, or the accumulator */
        o = by_name(name, M65_IMP);
        if (!o)
            o = by_name(name, M65_ACC);
        if (!o)
            asm_error(a, ASM_ERR_MISSING);
    } else if (asm_upper(*p) == 'A' && !asm_is_name_char(p[1]) && *asm_skip_space(p + 1) == '\0') {
        o = by_name(name, M65_ACC);
        if (!o)
            asm_error(a, ASM_ERR_MODE);
        p = asm_skip_space(p + 1);                       /* past the A */
    } else if (*p == '#') {                             /* immediate */
        uint8_t byte;

        p++;
        o = by_name(name, M65_IMM);
        if (!o) {
            asm_error(a, ASM_ERR_MODE);
        } else if (asm_expression(a, &p, &v) && asm_at_end(a, p) && asm_byte_value(a, v, &byte)) {
            asm_emit(a, o->opcode);
            asm_emit(a, byte);
        }
        return true;
    } else if (*p == '(') {                             /* indirect */
        char r;

        p++;
        if (!asm_expression(a, &p, &v))
            return true;
        p = asm_skip_space(p);
        if (*p == ',') {                                /* ($12,X) */
            const char *q = p;

            r = index_register(&q);
            q = asm_skip_space(q);
            if (r != 'X' || *q != ')') {
                asm_error(a, ASM_ERR_MODE);
                return true;
            }
            p = q + 1;
            o = by_name(name, M65_IZX);
            base = M65_IZX;
        } else if (*p == ')') {
            p = asm_skip_space(p + 1);
            if (*p == ',') {                            /* ($12),Y */
                r = index_register(&p);
                if (r != 'Y') {
                    asm_error(a, ASM_ERR_MODE);
                    return true;
                }
                o = by_name(name, M65_IZY);
                base = M65_IZY;
            } else {
                o = by_name(name, M65_IND);             /* ($1234) */
                base = M65_IND;
            }
        } else {
            asm_error(a, ASM_ERR_MODE);
            return true;
        }
        if (!o || !asm_at_end(a, p)) {
            if (!o)
                asm_error(a, ASM_ERR_MODE);
            return true;
        }
        if (base == M65_IND) {
            asm_emit(a, o->opcode);
            asm_emit(a, (uint8_t)(v & 0xFFu));
            asm_emit(a, (uint8_t)(v >> 8));
        } else {
            uint8_t byte;

            if (asm_byte_value(a, v, &byte)) {
                asm_emit(a, o->opcode);
                asm_emit(a, byte);
            }
        }
        return true;
    } else {                                            /* a value, and an index */
        char r;

        if (!asm_expression(a, &p, &v))
            return true;
        r = index_register(&p);
        if (!asm_at_end(a, p))
            return true;
        if (by_name(name, M65_REL) && r == 0) {         /* a branch to an address */
            long offset = (long)v - (long)(asm_location(a) + 2u);

            o = by_name(name, M65_REL);
            if (asm_pass(a) == 2 && (offset < -128 || offset > 127)) {
                asm_error(a, ASM_ERR_RANGE);
                return true;
            }
            asm_emit(a, o->opcode);
            asm_emit(a, (uint8_t)(offset & 0xFF));
            return true;
        }
        {
            int zp = r == 'X' ? M65_ZPX : (r == 'Y' ? M65_ZPY : M65_ZP);
            int ab = r == 'X' ? M65_ABX : (r == 'Y' ? M65_ABY : M65_ABS);
            bool short_ok = by_name(name, zp) != NULL && suffix != 'W' && v < 0x100u &&
                            (suffix == 'B' || !asm_forward(a));

            if (suffix == 'B' && (v >= 0x100u || !by_name(name, zp))) {
                asm_error(a, ASM_ERR_MODE);
                return true;
            }
            o = short_ok ? by_name(name, zp) : by_name(name, ab);
            if (!o) {
                asm_error(a, ASM_ERR_MODE);
                return true;
            }
            asm_emit(a, o->opcode);
            asm_emit(a, (uint8_t)(v & 0xFFu));
            if (o->mode == ab)
                asm_emit(a, (uint8_t)(v >> 8));
        }
        return true;
    }

    if (o) {                                            /* implied or accumulator */
        if (asm_at_end(a, p))
            asm_emit(a, o->opcode);
    }
    return true;
}

static bool reserved(const char *name)
{
    return (name[0] == 'A' || name[0] == 'X' || name[0] == 'Y') && name[1] == '\0';
}

const isa_t isa_6502 = {
    "6502",
    3,
    false,                                                  /* DW: low byte first */
    ISA_NUM_DOLLAR_HEX | ISA_NUM_PERCENT_BINARY | ISA_NUM_0X,
    disasm,
    flow,
    NULL,
    encode,
    reserved,
    NULL,
    NULL
};
