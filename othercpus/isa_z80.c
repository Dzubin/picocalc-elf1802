/*
 * isa_z80.c - the Zilog Z80's instruction set for the tools: how to write an
 * instruction, which way the program goes after it, and how to turn assembly
 * text into bytes. Zilog's mnemonics (LD A,(IX+5)) with this project's number
 * spelling for the 8080 family (12H, 0FFH, a bare number is decimal), documented
 * opcodes only (isa_z80_const.h).
 *
 * One decoder (decode_any) turns the bytes of an instruction into a mnemonic and
 * two operand texts; the disassembler writes those out, and the assembler runs the
 * decoder over every opcode, keeps the ones with the mnemonic it was given and
 * matches the operand text against the operand templates. The tables and the
 * assembler therefore cannot disagree.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "asm_core.h"
#include "isa.h"
#include "isa_others.h"
#include "isa_z80_const.h"

static const char *const regs[8]      = ISAZ80_REGS;
static const char *const pairs[4]     = ISAZ80_PAIRS;
static const char *const pairs_af[4]  = ISAZ80_PAIRS_AF;
static const char *const conds[8]     = ISAZ80_CONDS;
static const char *const alu_names[8] = ISAZ80_ALU;
static const char *const rot_names[8] = ISAZ80_ROT;
static const char *const acc_names[8] = ISAZ80_ACC;
static const char *const bit_names[8] = ISAZ80_BITS;
static const char *const rst_names[8] = ISAZ80_RST;
static const char *const block_names[4][4] = ISAZ80_BLOCK;
static const char *const reserved_names[] = ISAZ80_RESERVED;

/* ------------------------------------------------------------------ */
/*  Decoding                                                            */
/* ------------------------------------------------------------------ */

/* One decoded instruction. */
typedef struct {
    const char *mnem;               /* ADD                                     */
    const char *op1, *op2;          /* operand texts ("" for none)             */
    int         len;                /* bytes in all                            */
    int         first;              /* where the bytes of the placeholders start */
    int         kind;               /* ISA_FLOW_*                              */
} z80_t;

/* The bytes that follow the opcode for the placeholders in an operand text. */
static int extra_bytes(const char *s)
{
    int n = 0;

    for (; *s; s++) {
        if (s[0] == 'n' && s[1] == 'n') {
            n += 2;
            s++;
        } else if (s[0] == 'n' || s[0] == 'e' || (s[0] == '+' && s[1] == 'd')) {
            n++;
            if (s[0] == '+')
                s++;
        }
    }
    return n;
}

/* Fill in an instruction with prefix_len prefix bytes before its opcode. */
static bool set(z80_t *z, int prefix_len, const char *mnem, const char *op1, const char *op2,
                int kind)
{
    z->mnem = mnem;
    z->op1 = op1 ? op1 : "";
    z->op2 = op2 ? op2 : "";
    z->first = prefix_len + 1;
    z->len = z->first + extra_bytes(z->op1) + extra_bytes(z->op2);
    z->kind = kind;
    return true;
}

/* The CB group: rotates and shifts (not the undocumented SLL), BIT, RES, SET. */
static bool decode_cb(const uint8_t *b, int prefix_len, z80_t *z)
{
    int x = b[0] >> 6, y = (b[0] >> 3) & 7, w = b[0] & 7;

    switch (x) {
    case 0:
        if (y == ISAZ80_ROT_SLL)
            return false;
        return set(z, prefix_len, rot_names[y], regs[w], NULL, ISA_FLOW_NEXT);
    case 1:
        return set(z, prefix_len, "BIT", bit_names[y], regs[w], ISA_FLOW_NEXT);
    case 2:
        return set(z, prefix_len, "RES", bit_names[y], regs[w], ISA_FLOW_NEXT);
    default:
        return set(z, prefix_len, "SET", bit_names[y], regs[w], ISA_FLOW_NEXT);
    }
}

/* The ED group.
 *
 * Author: Thomas Dzubin */
static bool decode_ed(const uint8_t *b, z80_t *z)
{
    int x = b[0] >> 6, y = (b[0] >> 3) & 7, w = b[0] & 7, p = y >> 1, q = y & 1;

    if (x == 2) {                                   /* LDI, CPIR, OTDR and the others */
        if (w < 4 && y >= 4)
            return set(z, 1, block_names[w][y - 4], NULL, NULL, ISA_FLOW_NEXT);
        return false;
    }
    if (x != 1)
        return false;
    switch (w) {
    case 0:
        return y != 6 && set(z, 1, "IN", regs[y], "(C)", ISA_FLOW_NEXT);
    case 1:
        return y != 6 && set(z, 1, "OUT", "(C)", regs[y], ISA_FLOW_NEXT);
    case 2:
        return set(z, 1, q ? "ADC" : "SBC", "HL", pairs[p], ISA_FLOW_NEXT);
    case 3:
        if (p == 2)                                 /* HL has the shorter 22 and 2A */
            return false;
        if (q)
            return set(z, 1, "LD", pairs[p], "(nn)", ISA_FLOW_NEXT);
        return set(z, 1, "LD", "(nn)", pairs[p], ISA_FLOW_NEXT);
    case 4:
        return y == 0 && set(z, 1, "NEG", NULL, NULL, ISA_FLOW_NEXT);
    case 5:
        if (y == 0)
            return set(z, 1, "RETN", NULL, NULL, ISA_FLOW_STOP);
        return y == 1 && set(z, 1, "RETI", NULL, NULL, ISA_FLOW_STOP);
    case 6:
        if (y == 0)
            return set(z, 1, "IM", "0", NULL, ISA_FLOW_NEXT);
        if (y == 2)
            return set(z, 1, "IM", "1", NULL, ISA_FLOW_NEXT);
        return y == 3 && set(z, 1, "IM", "2", NULL, ISA_FLOW_NEXT);
    default:
        switch (y) {
        case 0: return set(z, 1, "LD", "I", "A", ISA_FLOW_NEXT);
        case 1: return set(z, 1, "LD", "R", "A", ISA_FLOW_NEXT);
        case 2: return set(z, 1, "LD", "A", "I", ISA_FLOW_NEXT);
        case 3: return set(z, 1, "LD", "A", "R", ISA_FLOW_NEXT);
        case 4: return set(z, 1, "RRD", NULL, NULL, ISA_FLOW_NEXT);
        case 5: return set(z, 1, "RLD", NULL, NULL, ISA_FLOW_NEXT);
        default: return false;
        }
    }
}

/* DD CB d op and FD CB d op: the rotates, BIT, RES and SET on (IX+d) or (IY+d).
 * b points at the CB; the displacement is next and the opcode last, and only
 * the documented forms (the byte at (IX+d) itself, bits 2-0 = 6) are accepted.
 *
 * Author: Thomas Dzubin */
static bool decode_index_cb(const uint8_t *b, const char *disp, z80_t *z)
{
    int x = b[2] >> 6, y = (b[2] >> 3) & 7;
    bool ok;

    if ((b[2] & 7) != 6)
        return false;
    switch (x) {
    case 0:
        ok = y != ISAZ80_ROT_SLL && set(z, 2, rot_names[y], disp, NULL, ISA_FLOW_NEXT);
        break;
    case 1:
        ok = set(z, 2, "BIT", bit_names[y], disp, ISA_FLOW_NEXT);
        break;
    case 2:
        ok = set(z, 2, "RES", bit_names[y], disp, ISA_FLOW_NEXT);
        break;
    default:
        ok = set(z, 2, "SET", bit_names[y], disp, ISA_FLOW_NEXT);
        break;
    }
    if (ok)
        z->first = 2;                               /* the displacement; the opcode comes after it */
    return ok;
}

/* The DD and FD groups: the instructions that name HL or (HL) with IX or IY in its
 * place. b points after the prefix.
 *
 * Author: Thomas Dzubin */
static bool decode_index(const uint8_t *b, bool iy, z80_t *z)
{
    const char *ix = iy ? "IY" : "IX";
    const char *disp = iy ? "(IY+d)" : "(IX+d)";
    const char *pairs_x[4];
    uint8_t op = b[0];
    int x = op >> 6, y = (op >> 3) & 7, w = op & 7, p = y >> 1;

    pairs_x[0] = pairs[0];
    pairs_x[1] = pairs[1];
    pairs_x[2] = ix;
    pairs_x[3] = pairs[3];
    switch (op) {
    case 0x09: case 0x19: case 0x29: case 0x39:
        return set(z, 1, "ADD", ix, pairs_x[p], ISA_FLOW_NEXT);
    case 0x21: return set(z, 1, "LD", ix, "nn", ISA_FLOW_NEXT);
    case 0x22: return set(z, 1, "LD", "(nn)", ix, ISA_FLOW_NEXT);
    case 0x2A: return set(z, 1, "LD", ix, "(nn)", ISA_FLOW_NEXT);
    case 0x23: return set(z, 1, "INC", ix, NULL, ISA_FLOW_NEXT);
    case 0x2B: return set(z, 1, "DEC", ix, NULL, ISA_FLOW_NEXT);
    case 0x34: return set(z, 1, "INC", disp, NULL, ISA_FLOW_NEXT);
    case 0x35: return set(z, 1, "DEC", disp, NULL, ISA_FLOW_NEXT);
    case 0x36: return set(z, 1, "LD", disp, "n", ISA_FLOW_NEXT);
    case 0xE1: return set(z, 1, "POP", ix, NULL, ISA_FLOW_NEXT);
    case 0xE3: return set(z, 1, "EX", "(SP)", ix, ISA_FLOW_NEXT);
    case 0xE5: return set(z, 1, "PUSH", ix, NULL, ISA_FLOW_NEXT);
    case 0xE9: return set(z, 1, "JP", iy ? "(IY)" : "(IX)", NULL, ISA_FLOW_STOP);
    case 0xF9: return set(z, 1, "LD", "SP", ix, ISA_FLOW_NEXT);
    case Z80_PREFIX_CB:
        return decode_index_cb(b, disp, z);
    default:
        break;
    }
    if (x == 1 && op != 0x76) {                     /* LD r,(IX+d) and LD (IX+d),r */
        if (w == 6 && y != 6)
            return set(z, 1, "LD", regs[y], disp, ISA_FLOW_NEXT);
        if (y == 6 && w != 6)
            return set(z, 1, "LD", disp, regs[w], ISA_FLOW_NEXT);
        return false;
    }
    if (x == 2 && w == 6) {                         /* ADD A,(IX+d) and the rest */
        if ((ISAZ80_ALU_WITH_A >> y) & 1u)
            return set(z, 1, alu_names[y], "A", disp, ISA_FLOW_NEXT);
        return set(z, 1, alu_names[y], disp, NULL, ISA_FLOW_NEXT);
    }
    return false;
}

/* Decode the instruction at b (up to four bytes are looked at); false if it is not
 * a documented one. The unprefixed opcodes are here, and the prefix bytes pass the
 * rest on to their groups.
 *
 * Author: Thomas Dzubin */
static bool decode_any(const uint8_t *b, z80_t *z)
{
    static const char *const store_op[4]  = { "(BC)", "(DE)", "(nn)", "(nn)" };
    static const char *const store_reg[4] = { "A", "A", "HL", "A" };
    uint8_t op = b[0];
    int x = op >> 6, y = (op >> 3) & 7, w = op & 7, p = y >> 1, q = y & 1;
    bool with_a = ((ISAZ80_ALU_WITH_A >> y) & 1u) != 0;

    switch (x) {
    case 0:
        switch (w) {
        case 0:
            if (y == 0)
                return set(z, 0, "NOP", NULL, NULL, ISA_FLOW_NEXT);
            if (y == 1)
                return set(z, 0, "EX", "AF", "AF'", ISA_FLOW_NEXT);
            if (y == 2)
                return set(z, 0, "DJNZ", "e", NULL, ISA_FLOW_BRANCH);
            if (y == 3)
                return set(z, 0, "JR", "e", NULL, ISA_FLOW_JUMP);
            return set(z, 0, "JR", conds[y - 4], "e", ISA_FLOW_BRANCH);
        case 1:
            if (q)
                return set(z, 0, "ADD", "HL", pairs[p], ISA_FLOW_NEXT);
            return set(z, 0, "LD", pairs[p], "nn", ISA_FLOW_NEXT);
        case 2:
            if (q)
                return set(z, 0, "LD", store_reg[p], store_op[p], ISA_FLOW_NEXT);
            return set(z, 0, "LD", store_op[p], store_reg[p], ISA_FLOW_NEXT);
        case 3:
            return set(z, 0, q ? "DEC" : "INC", pairs[p], NULL, ISA_FLOW_NEXT);
        case 4:
            return set(z, 0, "INC", regs[y], NULL, ISA_FLOW_NEXT);
        case 5:
            return set(z, 0, "DEC", regs[y], NULL, ISA_FLOW_NEXT);
        case 6:
            return set(z, 0, "LD", regs[y], "n", ISA_FLOW_NEXT);
        default:
            return set(z, 0, acc_names[y], NULL, NULL, ISA_FLOW_NEXT);
        }
    case 1:
        if (op == 0x76)
            return set(z, 0, "HALT", NULL, NULL, ISA_FLOW_STOP);
        return set(z, 0, "LD", regs[y], regs[w], ISA_FLOW_NEXT);
    case 2:
        return set(z, 0, alu_names[y], with_a ? "A" : regs[w], with_a ? regs[w] : NULL,
                   ISA_FLOW_NEXT);
    default:
        break;
    }
    switch (w) {                                    /* x == 3 */
    case 0:
        return set(z, 0, "RET", conds[y], NULL, ISA_FLOW_NEXT);
    case 1:
        if (!q)
            return set(z, 0, "POP", pairs_af[p], NULL, ISA_FLOW_NEXT);
        switch (p) {
        case 0: return set(z, 0, "RET", NULL, NULL, ISA_FLOW_STOP);
        case 1: return set(z, 0, "EXX", NULL, NULL, ISA_FLOW_NEXT);
        case 2: return set(z, 0, "JP", "(HL)", NULL, ISA_FLOW_STOP);
        default: return set(z, 0, "LD", "SP", "HL", ISA_FLOW_NEXT);
        }
    case 2:
        return set(z, 0, "JP", conds[y], "nn", ISA_FLOW_BRANCH);
    case 3:
        switch (y) {
        case 0: return set(z, 0, "JP", "nn", NULL, ISA_FLOW_JUMP);
        case 1: return decode_cb(b + 1, 1, z);
        case 2: return set(z, 0, "OUT", "(n)", "A", ISA_FLOW_NEXT);
        case 3: return set(z, 0, "IN", "A", "(n)", ISA_FLOW_NEXT);
        case 4: return set(z, 0, "EX", "(SP)", "HL", ISA_FLOW_NEXT);
        case 5: return set(z, 0, "EX", "DE", "HL", ISA_FLOW_NEXT);
        case 6: return set(z, 0, "DI", NULL, NULL, ISA_FLOW_NEXT);
        default: return set(z, 0, "EI", NULL, NULL, ISA_FLOW_NEXT);
        }
    case 4:
        return set(z, 0, "CALL", conds[y], "nn", ISA_FLOW_CALL);
    case 5:
        if (!q)
            return set(z, 0, "PUSH", pairs_af[p], NULL, ISA_FLOW_NEXT);
        switch (p) {
        case 0: return set(z, 0, "CALL", "nn", NULL, ISA_FLOW_CALL);
        case 1: return decode_index(b + 1, false, z);
        case 2: return decode_ed(b + 1, z);
        default: return decode_index(b + 1, true, z);
        }
    case 6:
        return set(z, 0, alu_names[y], with_a ? "A" : "n", with_a ? "n" : NULL, ISA_FLOW_NEXT);
    default:
        return set(z, 0, "RST", rst_names[y], NULL, ISA_FLOW_CALL);
    }
}

/* ------------------------------------------------------------------ */
/*  Disassembling                                                       */
/* ------------------------------------------------------------------ */
/* A number in the 8080 family's spelling: 12H, 0FFH (a digit first). */
static void hexh(char *dst, size_t size, unsigned value, int digits)
{
    char digs[8];

    snprintf(digs, sizeof digs, "%0*X", digits, value);
    snprintf(dst, size, "%s%sH", digs[0] > '9' ? "0" : "", digs);
}

static void append(char *dst, size_t size, const char *s)
{
    size_t n = strlen(dst);

    snprintf(dst + n, size - n, "%s", s);
}

/* Write an operand text with the values that follow the opcode in place of its
 * placeholders; *args moves past the bytes used. A branch or jump target text
 * (from the listing's labels) takes the place of the address if one is given.
 *
 * Author: Thomas Dzubin */
static void expand(char *dst, size_t size, const char *t, const uint8_t **args, unsigned addr,
                   int len, const char *target)
{
    char num[12];

    dst[0] = '\0';
    for (; *t; t++) {
        if (t[0] == '+' && t[1] == 'd') {
            int d = (int8_t)*(*args)++;

            hexh(num, sizeof num, (unsigned)(d < 0 ? -d : d), 2);
            append(dst, size, d < 0 ? "-" : "+");
            append(dst, size, num);
            t++;
        } else if (t[0] == 'n' && t[1] == 'n') {
            if (target) {
                append(dst, size, target);
            } else {
                hexh(num, sizeof num, (unsigned)((*args)[0] | ((*args)[1] << 8)), 4);
                append(dst, size, num);
            }
            *args += 2;
            t++;
        } else if (t[0] == 'n') {
            hexh(num, sizeof num, *(*args)++, 2);
            append(dst, size, num);
        } else if (t[0] == 'e') {
            if (target) {
                append(dst, size, target);
            } else {
                unsigned dest = (addr + (unsigned)len + (unsigned)(int)(int8_t)**args) & 0xFFFFu;

                hexh(num, sizeof num, dest, 4);
                append(dst, size, num);
            }
            (*args)++;
        } else {
            char one[2] = { *t, '\0' };

            append(dst, size, one);
        }
    }
}

/* Author: Thomas Dzubin */
static int disasm(uint16_t addr, const uint8_t *b, const char *target, char *text, size_t size)
{
    z80_t z;
    const uint8_t *args;
    char o1[Z80_OPERAND_MAX], o2[Z80_OPERAND_MAX], num[12];
    bool branch;

    if (!decode_any(b, &z)) {
        hexh(num, sizeof num, b[0], 2);
        snprintf(text, size, "DB %s", num);
        return 1;
    }
    branch = z.kind == ISA_FLOW_JUMP || z.kind == ISA_FLOW_BRANCH || z.kind == ISA_FLOW_CALL;
    args = b + z.first;
    expand(o1, sizeof o1, z.op1, &args, addr, z.len, branch ? target : NULL);
    expand(o2, sizeof o2, z.op2, &args, addr, z.len, branch ? target : NULL);
    if (o1[0] == '\0')
        snprintf(text, size, "%s", z.mnem);
    else if (o2[0] == '\0')
        snprintf(text, size, "%-4s %s", z.mnem, o1);
    else
        snprintf(text, size, "%-4s %s,%s", z.mnem, o1, o2);
    return z.len;
}

/* Author: Thomas Dzubin */
static void flow(uint16_t addr, const uint8_t *b, isa_flow_t *f)
{
    z80_t z;

    f->target = -1;
    f->skip = 0;
    if (!decode_any(b, &z)) {
        f->length = 1;
        f->kind = ISA_FLOW_BAD;
        return;
    }
    f->length = z.len;
    f->kind = z.kind;
    if (z.kind != ISA_FLOW_JUMP && z.kind != ISA_FLOW_BRANCH && z.kind != ISA_FLOW_CALL)
        return;
    if (strchr(z.op1, 'e') || strchr(z.op2, 'e'))
        f->target = (int)((addr + (unsigned)z.len + (unsigned)(int)(int8_t)b[z.first]) & 0xFFFFu);
    else if (strstr(z.op1, "nn") || strstr(z.op2, "nn"))
        f->target = (int)((unsigned)b[z.first] | ((unsigned)b[z.first + 1] << 8));
    else if (strcmp(z.mnem, "RST") == 0)
        f->target = (int)(b[0] & 0x38u);
}

/* ------------------------------------------------------------------ */
/*  Assembling                                                          */
/* ------------------------------------------------------------------ */

/* Is the word one that names a register or a condition? */
static bool is_reserved(const char *word)
{
    size_t i;

    for (i = 0; i < sizeof reserved_names / sizeof reserved_names[0]; i++)
        if (strcmp(word, reserved_names[i]) == 0)
            return true;
    return false;
}

/* One placeholder found in the operand text, with the text it stands for. */
typedef struct {
    char        kind;               /* 'n' byte, 'N' word, 'e' relative, 'd' displacement */
    const char *start, *end;
} found_t;

/* Can the text from s to end be a number or a label, as opposed to a register
 * or something in parentheses? */
static bool value_text(const char *s, const char *end)
{
    char word[ISAZ80_RESERVED_MAX + 2];
    size_t n = 0;

    if (s >= end || *s == '(')
        return false;
    while (s + n < end && asm_is_name_char(s[n]) && n <= ISAZ80_RESERVED_MAX) {
        word[n] = asm_upper(s[n]);
        n++;
    }
    word[n] = '\0';
    if (n <= ISAZ80_RESERVED_MAX && (s + n >= end || !asm_is_name_char(s[n])))
        return !is_reserved(word);
    return true;
}

/* Where the text of a placeholder ends: at the character stop (the one the
 * template has next), or at the end of the line. A quoted character is passed
 * over whole. Spaces at the end are not part of it. */
static const char *text_end(const char *start, char stop)
{
    const char *p = start, *e;

    while (*p && asm_upper(*p) != stop) {
        if (*p == '\'' && p[1] && p[2] == '\'')
            p += 2;
        p++;
    }
    e = p;
    while (e > start && e[-1] == ' ')
        e--;
    return e;
}

/* Does the operand text match the template (an instruction's two operand texts
 * with a comma between)? Registers and punctuation must be the same, the
 * placeholders must stand for something that can be a value, and nothing may be
 * left over. The placeholders found are stored in order; nothing is evaluated
 * here, so no error is noted for a candidate that turns out not to fit.
 *
 * Author: Thomas Dzubin */
static bool match(const char *tpl, const char *in, found_t *found, int *count)
{
    const char *p = asm_skip_space(in);

    *count = 0;
    while (*tpl) {
        found_t *f;
        char kind = 0;
        int width = 1;

        if (tpl[0] == '+' && tpl[1] == 'd') {
            kind = 'd';
            width = 2;
        } else if (tpl[0] == 'n' && tpl[1] == 'n') {
            kind = 'N';
            width = 2;
        } else if (tpl[0] == 'n' || tpl[0] == 'e') {
            kind = tpl[0];
        } else {
            p = asm_skip_space(p);
            if (asm_upper(*p) != *tpl)
                return false;
            p++;
            tpl++;
            continue;
        }
        if (*count >= Z80_PLACEHOLDERS)
            return false;
        f = &found[*count];
        f->kind = kind;
        p = asm_skip_space(p);
        if (f->kind == 'd' && *p == '+')
            p = asm_skip_space(p + 1);              /* a '-' is left for the number */
        f->start = p;
        p = text_end(p, tpl[width] ? tpl[width] : '\0');
        f->end = p;
        if (f->kind == 'd' && f->start == f->end) {
            /* (IX) is (IX+0) */
        } else if (!value_text(f->start, f->end)) {
            return false;
        }
        (*count)++;
        tpl += width;
    }
    return *asm_skip_space(p) == '\0';
}

/* The value a placeholder stands for, worked out now (this notes errors).
 * Returns false if there is an error.
 *
 * Author: Thomas Dzubin */
static bool evaluate(asm_t *a, const found_t *f, int len, uint32_t *out)
{
    const char *q = f->start;
    uint32_t v = 0;
    uint8_t byte;

    if (f->kind == 'd' && f->start == f->end) {
        *out = 0;
        return true;
    }
    if (!asm_expression(a, &q, &v))
        return false;
    if (asm_skip_space(q) < f->end) {
        asm_error(a, ASM_ERR_EXTRA);
        return false;
    }
    switch (f->kind) {
    case 'n':
        if (!asm_byte_value(a, v, &byte))
            return false;
        *out = byte;
        break;
    case 'd':
        if (v > 0x7Fu && v < 0xFF80u) {
            asm_error(a, ASM_ERR_BIG);
            return false;
        }
        *out = v & 0xFFu;
        break;
    case 'e': {
        long offset = (long)v - (long)(asm_location(a) + (uint32_t)len);

        if (asm_pass(a) == 2 && (offset < -128 || offset > 127)) {
            asm_error(a, ASM_ERR_RANGE);
            return false;
        }
        *out = (uint32_t)offset & 0xFFu;
        break;
    }
    default:
        *out = v & 0xFFFFu;
        break;
    }
    return true;
}

/* The prefix bytes of an opcode table, and the bytes to hand the decoder to
 * ask what an opcode of that table is. Returns how many prefix bytes there are.
 *
 * Author: Thomas Dzubin */
static int table_bytes(int table, uint8_t op, uint8_t *b)
{
    memset(b, 0, 4);
    switch (table) {
    case Z80_TABLE_CB:
        b[0] = Z80_PREFIX_CB;
        b[1] = op;
        return 1;
    case Z80_TABLE_ED:
        b[0] = Z80_PREFIX_ED;
        b[1] = op;
        return 1;
    case Z80_TABLE_DD:
    case Z80_TABLE_FD:
        b[0] = table == Z80_TABLE_DD ? Z80_PREFIX_DD : Z80_PREFIX_FD;
        b[1] = op;
        return 1;
    case Z80_TABLE_DDCB:
    case Z80_TABLE_FDCB:
        b[0] = table == Z80_TABLE_DDCB ? Z80_PREFIX_DD : Z80_PREFIX_FD;
        b[1] = Z80_PREFIX_CB;
        b[3] = op;                                  /* b[2] is the displacement */
        return 2;
    default:
        b[0] = op;
        return 0;
    }
}

/* Put the bytes of the chosen instruction in the image: the prefix bytes, the
 * opcode and the bytes of the placeholders (a word is two of them, low first). In
 * the DD CB and FD CB groups the displacement comes before the opcode. */
static void emit_instruction(asm_t *a, int table, uint8_t op, const uint32_t *values, int count)
{
    uint8_t b[4];
    int prefix_len = table_bytes(table, op, b), i;

    for (i = 0; i < prefix_len; i++)
        asm_emit(a, b[i]);
    if (prefix_len == 2) {
        asm_emit(a, (uint8_t)values[0]);
        asm_emit(a, op);
        return;
    }
    asm_emit(a, op);
    for (i = 0; i < count; i++)
        asm_emit(a, (uint8_t)values[i]);
}

/* RST takes the restart address (0, 8, ... 38H). */
static bool encode_rst(asm_t *a, const char *operands)
{
    const char *p = operands;
    uint32_t v;

    if (asm_expression(a, &p, &v) && asm_at_end(a, p)) {
        if (v > 0x38u || (v & 7u) != 0)
            asm_error(a, ASM_ERR_MODE);
        else
            asm_emit(a, (uint8_t)(0xC7u | v));
    }
    return true;
}

/* Author: Thomas Dzubin */
static bool encode(asm_t *a, const char *mnemonic, const char *operands)
{
    bool known = false;
    int table, op;

    if (strcmp(mnemonic, "RST") == 0)
        return encode_rst(a, operands);
    for (table = 0; table < Z80_TABLE_COUNT; table++) {
        for (op = 0; op < 256; op++) {
            uint8_t b[4];
            z80_t z;
            found_t found[Z80_PLACEHOLDERS];
            uint32_t values[2 * Z80_PLACEHOLDERS];
            char tpl[Z80_TEMPLATE_MAX];
            int count, i, out = 0;

            table_bytes(table, (uint8_t)op, b);
            if (table == Z80_TABLE_BASE && (op == Z80_PREFIX_CB || op == Z80_PREFIX_ED ||
                                            op == Z80_PREFIX_DD || op == Z80_PREFIX_FD))
                continue;
            if (!decode_any(b, &z) || strcmp(z.mnem, mnemonic) != 0)
                continue;
            known = true;
            snprintf(tpl, sizeof tpl, "%s%s%s", z.op1, z.op2[0] ? "," : "", z.op2);
            if (!match(tpl, operands, found, &count))
                continue;
            for (i = 0; i < count; i++) {
                uint32_t v;

                if (!evaluate(a, &found[i], z.len, &v))
                    return true;
                values[out++] = v;
                if (found[i].kind == 'N')
                    values[out++] = v >> 8;         /* a word is two bytes, low first */
            }
            emit_instruction(a, table, (uint8_t)op, values, out);
            return true;
        }
    }
    if (known)
        asm_error(a, ASM_ERR_MODE);
    return known;
}

/* The registers and conditions cannot be labels. */
static bool reserved(const char *name)
{
    return is_reserved(name);
}

const isa_t isa_z80 = {
    "Z80",
    4,
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
