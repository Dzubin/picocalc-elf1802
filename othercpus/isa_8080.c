/*
 * isa_8080.c - the Intel 8080's instruction set for the tools: how to write an
 * instruction, which way the program goes after it, and how to turn assembly
 * text into bytes. Intel's mnemonics and number spelling (12H, 0FFH, a bare number
 * is decimal), documented opcodes only (isa_8080_const.h).
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "asm_core.h"
#include "isa.h"
#include "isa_others.h"
#include "isa_8080_const.h"

typedef struct {
    const char *name;
    uint8_t     base;
    uint8_t     kind;
} op_t;

static const op_t ops[] = ISA8080_OPS;
#define OP_COUNT (sizeof ops / sizeof ops[0])

static const char *const reg_names[8]    = ISA8080_REGS;
static const char *const pair_names[4]   = ISA8080_PAIRS;
static const char *const pair_p_names[4] = ISA8080_PAIRS_P;

/* The opcode bits that select the instruction; the others are its operands. */
static uint8_t kind_mask(uint8_t kind)
{
    switch (kind) {
    case K80_R3:    return 0xF8;
    case K80_R3D:   return 0xC7;
    case K80_MOV:   return 0xC0;
    case K80_MVI:   return 0xC7;
    case K80_RP:    return 0xCF;
    case K80_LXI:   return 0xCF;
    case K80_STAX:  return 0xEF;
    case K80_RPP:   return 0xCF;
    case K80_RST:   return 0xC7;
    default:        return 0xFF;
    }
}

static const op_t *by_opcode(uint8_t opcode)
{
    size_t i;

    for (i = 0; i < OP_COUNT; i++)
        if ((opcode & kind_mask(ops[i].kind)) == ops[i].base)
            return &ops[i];
    return NULL;
}

static const op_t *by_name(const char *name)
{
    size_t i;

    for (i = 0; i < OP_COUNT; i++)
        if (strcmp(ops[i].name, name) == 0)
            return &ops[i];
    return NULL;
}

static int op_length(const op_t *o)
{
    switch (o->kind) {
    case K80_MVI:
    case K80_D8:
    case K80_PORT:
        return 2;
    case K80_LXI:
    case K80_A16:
        return 3;
    default:
        return 1;
    }
}

/* ------------------------------------------------------------------ */
/*  Disassembling                                                       */
/* ------------------------------------------------------------------ */
/* A number in Intel's spelling: 12H, 0FFH (a digit first). */
static void hexh(char *dst, size_t size, unsigned value, int digits)
{
    char digs[8];

    snprintf(digs, sizeof digs, "%0*X", digits, value);
    snprintf(dst, size, "%s%sH", digs[0] > '9' ? "0" : "", digs);
}

/* Author: Thomas Dzubin */
static int disasm(uint16_t addr, const uint8_t *b, const char *target, char *text, size_t size)
{
    const op_t *o = by_opcode(b[0]);
    char num[12];
    unsigned w = (unsigned)b[1] | ((unsigned)b[2] << 8);

    (void)addr;
    if (!o) {
        snprintf(text, size, "DB %02X", b[0]);
        return 1;
    }
    switch (o->kind) {
    case K80_NONE:
        snprintf(text, size, "%s", o->name);
        break;
    case K80_R3:
        snprintf(text, size, "%-4s %s", o->name, reg_names[b[0] & 7]);
        break;
    case K80_R3D:
        snprintf(text, size, "%-4s %s", o->name, reg_names[(b[0] >> 3) & 7]);
        break;
    case K80_MOV:
        snprintf(text, size, "%-4s %s,%s", o->name, reg_names[(b[0] >> 3) & 7], reg_names[b[0] & 7]);
        break;
    case K80_MVI:
        hexh(num, sizeof num, b[1], 2);
        snprintf(text, size, "%-4s %s,%s", o->name, reg_names[(b[0] >> 3) & 7], num);
        break;
    case K80_RP:
        snprintf(text, size, "%-4s %s", o->name, pair_names[(b[0] >> 4) & 3]);
        break;
    case K80_LXI:
        hexh(num, sizeof num, w, 4);
        snprintf(text, size, "%-4s %s,%s", o->name, pair_names[(b[0] >> 4) & 3], num);
        break;
    case K80_STAX:
        snprintf(text, size, "%-4s %s", o->name, (b[0] & 0x10) ? "D" : "B");
        break;
    case K80_RPP:
        snprintf(text, size, "%-4s %s", o->name, pair_p_names[(b[0] >> 4) & 3]);
        break;
    case K80_D8:
    case K80_PORT:
        hexh(num, sizeof num, b[1], 2);
        snprintf(text, size, "%-4s %s", o->name, num);
        break;
    case K80_A16:
        if (target) {
            snprintf(text, size, "%-4s %s", o->name, target);
        } else {
            hexh(num, sizeof num, w, 4);
            snprintf(text, size, "%-4s %s", o->name, num);
        }
        break;
    default:                                            /* K80_RST */
        snprintf(text, size, "%-4s %d", o->name, (b[0] >> 3) & 7);
        break;
    }
    return op_length(o);
}

/* Author: Thomas Dzubin */
static void flow(uint16_t addr, const uint8_t *b, isa_flow_t *f)
{
    const op_t *o = by_opcode(b[0]);

    (void)addr;
    f->target = -1;
    f->skip = 0;
    f->kind = ISA_FLOW_NEXT;
    if (!o) {
        f->length = 1;
        f->kind = ISA_FLOW_BAD;
        return;
    }
    f->length = op_length(o);
    if (o->kind == K80_A16) {
        unsigned w = (unsigned)b[1] | ((unsigned)b[2] << 8);

        if (strcmp(o->name, "JMP") == 0) {
            f->kind = ISA_FLOW_JUMP;
            f->target = (int)w;
        } else if (o->name[0] == 'J') {
            f->kind = ISA_FLOW_BRANCH;
            f->target = (int)w;
        } else if (o->name[0] == 'C') {                 /* CALL and the conditional calls */
            f->kind = ISA_FLOW_CALL;
            f->target = (int)w;
        }
    } else if (o->kind == K80_RST) {
        f->kind = ISA_FLOW_CALL;
        f->target = (int)(b[0] & 0x38u);
    } else if (strcmp(o->name, "RET") == 0 || strcmp(o->name, "PCHL") == 0 ||
               strcmp(o->name, "HLT") == 0) {
        f->kind = ISA_FLOW_STOP;
    }
}

/* ------------------------------------------------------------------ */
/*  Assembling                                                          */
/* ------------------------------------------------------------------ */
/* A register or pair name at *p, matched against names; moves p past it. */
static int read_name(const char **p, const char *const *names, int count)
{
    const char *q = asm_skip_space(*p);
    char word[8];
    int i;

    if (!asm_is_name_start(*q))
        return -1;
    asm_read_word(&q, word, sizeof word);
    for (i = 0; i < count; i++)
        if (strcmp(word, names[i]) == 0) {
            *p = q;
            return i;
        }
    return -1;
}

/* A comma between operands. */
static bool comma(asm_t *a, const char **p)
{
    const char *q = asm_skip_space(*p);

    if (*q != ',') {
        asm_error(a, ASM_ERR_MODE);
        return false;
    }
    *p = q + 1;
    return true;
}

/* Author: Thomas Dzubin */
static bool encode(asm_t *a, const char *mnemonic, const char *operands)
{
    const op_t *o = by_name(mnemonic);
    const char *p = operands;
    int r1 = 0, r2 = 0;
    uint32_t v = 0;
    uint8_t byte;
    uint8_t opcode;

    if (!o)
        return false;
    opcode = o->base;
    switch (o->kind) {
    case K80_NONE:
        if (asm_at_end(a, p))
            asm_emit(a, opcode);
        return true;
    case K80_R3:
    case K80_R3D:
        r1 = read_name(&p, reg_names, 8);
        if (r1 < 0 || !asm_at_end(a, p)) {
            if (r1 < 0)
                asm_error(a, ASM_ERR_MODE);
            return true;
        }
        asm_emit(a, (uint8_t)(opcode | (o->kind == K80_R3 ? r1 : r1 << 3)));
        return true;
    case K80_MOV:
        r1 = read_name(&p, reg_names, 8);
        if (r1 < 0 || !comma(a, &p))
            { if (r1 < 0) asm_error(a, ASM_ERR_MODE); return true; }
        r2 = read_name(&p, reg_names, 8);
        if (r2 < 0 || !asm_at_end(a, p))
            { if (r2 < 0) asm_error(a, ASM_ERR_MODE); return true; }
        if (r1 == 6 && r2 == 6) {                       /* MOV M,M is HLT */
            asm_error(a, ASM_ERR_MODE);
            return true;
        }
        asm_emit(a, (uint8_t)(opcode | (r1 << 3) | r2));
        return true;
    case K80_MVI:
        r1 = read_name(&p, reg_names, 8);
        if (r1 < 0 || !comma(a, &p))
            { if (r1 < 0) asm_error(a, ASM_ERR_MODE); return true; }
        if (asm_expression(a, &p, &v) && asm_at_end(a, p) && asm_byte_value(a, v, &byte)) {
            asm_emit(a, (uint8_t)(opcode | (r1 << 3)));
            asm_emit(a, byte);
        }
        return true;
    case K80_RP:
    case K80_RPP:
    case K80_LXI:
        r1 = read_name(&p, o->kind == K80_RPP ? pair_p_names : pair_names, 4);
        if (r1 < 0) {
            asm_error(a, ASM_ERR_MODE);
            return true;
        }
        if (o->kind == K80_LXI) {
            if (!comma(a, &p) || !asm_expression(a, &p, &v) || !asm_at_end(a, p))
                return true;
            asm_emit(a, (uint8_t)(opcode | (r1 << 4)));
            asm_emit(a, (uint8_t)(v & 0xFFu));
            asm_emit(a, (uint8_t)(v >> 8));
        } else if (asm_at_end(a, p)) {
            asm_emit(a, (uint8_t)(opcode | (r1 << 4)));
        }
        return true;
    case K80_STAX:
        r1 = read_name(&p, pair_names, 2);              /* B or D */
        if (r1 < 0 || !asm_at_end(a, p)) {
            if (r1 < 0)
                asm_error(a, ASM_ERR_MODE);
            return true;
        }
        asm_emit(a, (uint8_t)(opcode | (r1 << 4)));
        return true;
    case K80_D8:
    case K80_PORT:
        if (asm_expression(a, &p, &v) && asm_at_end(a, p) && asm_byte_value(a, v, &byte)) {
            asm_emit(a, opcode);
            asm_emit(a, byte);
        }
        return true;
    case K80_A16:
        if (asm_expression(a, &p, &v) && asm_at_end(a, p)) {
            asm_emit(a, opcode);
            asm_emit(a, (uint8_t)(v & 0xFFu));
            asm_emit(a, (uint8_t)(v >> 8));
        }
        return true;
    default:                                            /* K80_RST */
        if (asm_expression(a, &p, &v) && asm_at_end(a, p)) {
            if (v > 7u)
                asm_error(a, ASM_ERR_MODE);
            else
                asm_emit(a, (uint8_t)(opcode | (v << 3)));
        }
        return true;
    }
}

/* The registers and pairs cannot be labels. */
static bool reserved(const char *name)
{
    int i;

    for (i = 0; i < 8; i++)
        if (strcmp(name, reg_names[i]) == 0)
            return true;
    return strcmp(name, "SP") == 0 || strcmp(name, "PSW") == 0;
}

const isa_t isa_8080 = {
    "8080",
    3,
    false,                                                  /* DW: low byte first */
    ISA_NUM_H_SUFFIX | ISA_NUM_B_SUFFIX,
    disasm,
    flow,
    NULL,
    encode,
    reserved,
    NULL,
    NULL
};
