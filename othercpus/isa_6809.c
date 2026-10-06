/*
 * isa_6809.c - the Motorola 6809's instruction set for the tools: how to write an
 * instruction, which way the program goes after it, and how to turn assembly text
 * into bytes (isa_6809_const.h has the table). Documented opcodes only.
 *
 * The source is written the usual way: LDA #$12, LDA $12, LDA 5,X, LDA ,X++,
 * LDA [,Y], LDD $1234, BNE label, TFR A,B, PSHS A,B,X. An operand below 256 that
 * is not a forward reference uses the direct form (the direct page register is
 * taken to be 0) and an indexed offset the shortest form it fits; a "<" in front
 * forces the short form (a direct address, an 8-bit offset), a ">" the long one
 * (an extended address, a 16-bit offset), and the listing writes the one a
 * program really has where the shortest would not read back the same.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "asm_core.h"
#include "isa.h"
#include "isa_others.h"
#include "isa_6809_const.h"

typedef struct {
    const char *name;
    uint8_t     mode;
} entry_t;

static entry_t pages[3][256];                           /* no prefix, 10, 11 */
static bool    built;

static void build(void)
{
    static const struct { uint16_t opcode; const char *name; uint8_t mode; } ops[] = ISA6809_OPS;
    size_t i;

    if (built)
        return;
    for (i = 0; i < sizeof ops / sizeof ops[0]; i++) {
        unsigned page = ops[i].opcode >> 8 == 0x10 ? 1u : (ops[i].opcode >> 8 == 0x11 ? 2u : 0u);

        pages[page][ops[i].opcode & 0xFFu].name = ops[i].name;
        pages[page][ops[i].opcode & 0xFFu].mode = ops[i].mode;
    }
    built = true;
}

/* The entry for the instruction at b, and how many bytes its opcode takes. */
static const entry_t *decode(const uint8_t *b, int *oplen)
{
    const entry_t *e;

    build();
    if (b[0] == 0x10 || b[0] == 0x11) {
        *oplen = 2;
        e = &pages[b[0] == 0x10 ? 1 : 2][b[1]];
    } else {
        *oplen = 1;
        e = &pages[0][b[0]];
    }
    return e->name ? e : NULL;
}

static const char *const rr_names[16]   = ISA6809_RR_NAMES;
static const char *const list_names[8]  = ISA6809_LIST_NAMES;

/* ------------------------------------------------------------------ */
/*  Indexed addressing: the postbyte                                    */
/* ------------------------------------------------------------------ */
/* How many bytes follow the postbyte, and whether it is a postbyte that the
 * 6809 has.
 *
 * Author: Thomas Dzubin */
static bool idx_extra(uint8_t pb, int *extra)
{
    bool ind = (pb & 0x10) != 0;

    *extra = 0;
    if (!(pb & 0x80))
        return true;                                    /* a 5-bit offset */
    switch (pb & 0x0F) {
    case 0x0:
    case 0x2:
        return !ind;                                    /* ,R+ and ,-R cannot be indirect */
    case 0x1: case 0x3: case 0x4: case 0x5: case 0x6: case 0xB:
        return true;
    case 0x8:
        *extra = 1;
        return true;
    case 0x9:
        *extra = 2;
        return true;
    case 0xC:                                           /* relative to the PC: register bits are zero */
        *extra = 1;
        return !(pb & 0x60);
    case 0xD:
        *extra = 2;
        return !(pb & 0x60);
    case 0xF:
        *extra = 2;
        return ind && !(pb & 0x60);                     /* [$1234] only, register bits zero */
    default:
        return false;
    }
}

/* ------------------------------------------------------------------ */
/*  Lengths and flow                                                    */
/* ------------------------------------------------------------------ */
/* The length of the instruction, or 0 if it has a postbyte or a register code
 * that the 6809 does not have (it is then data).
 *
 * Author: Thomas Dzubin */
static int instruction_length(const entry_t *e, const uint8_t *b, int oplen)
{
    int extra;

    switch (e->mode) {
    case M09_INH:
        return oplen;
    case M09_IMM8:
    case M09_DIR:
    case M09_REL:
        return oplen + 1;
    case M09_IMM16:
    case M09_EXT:
    case M09_LREL:
        return oplen + 2;
    case M09_RR:
        return rr_names[b[oplen] >> 4] && rr_names[b[oplen] & 15] &&
               ((b[oplen] >> 4) < 8) == ((b[oplen] & 15) < 8) ? oplen + 1 : 0;
    case M09_IDX:
        if (!idx_extra(b[oplen], &extra))
            return 0;
        return oplen + 1 + extra;
    default:                                            /* PSHS, PULS, PSHU, PULU */
        return oplen + 1;
    }
}

/* Author: Thomas Dzubin */
static void flow(uint16_t addr, const uint8_t *b, isa_flow_t *f)
{
    int oplen;
    const entry_t *e = decode(b, &oplen);
    int len;

    f->target = -1;
    f->skip = 0;
    f->kind = ISA_FLOW_NEXT;
    if (!e) {
        f->length = 1;
        f->kind = ISA_FLOW_BAD;
        return;
    }
    len = instruction_length(e, b, oplen);
    if (len == 0) {                                     /* a postbyte or register it does not have */
        f->length = oplen + 1;
        f->kind = ISA_FLOW_DATA;
        return;
    }
    f->length = len;
    if (e->mode == M09_REL) {
        f->target = (int)(((unsigned)addr + (unsigned)len + (unsigned)(int8_t)b[oplen]) & 0xFFFFu);
    } else if (e->mode == M09_LREL) {
        f->target = (int)(((unsigned)addr + (unsigned)len +
                           (unsigned)(int16_t)(((unsigned)b[oplen] << 8) | b[oplen + 1])) & 0xFFFFu);
    }
    if (e->mode == M09_REL || e->mode == M09_LREL) {
        if (strcmp(e->name, "BRA") == 0 || strcmp(e->name, "LBRA") == 0)
            f->kind = ISA_FLOW_JUMP;
        else if (strcmp(e->name, "BSR") == 0 || strcmp(e->name, "LBSR") == 0)
            f->kind = ISA_FLOW_CALL;
        else if (strcmp(e->name, "BRN") != 0 && strcmp(e->name, "LBRN") != 0)
            f->kind = ISA_FLOW_BRANCH;
    } else if (strcmp(e->name, "JMP") == 0) {
        if (e->mode == M09_EXT) {
            f->kind = ISA_FLOW_JUMP;
            f->target = (int)(((unsigned)b[oplen] << 8) | b[oplen + 1]);
        } else if (e->mode == M09_DIR) {
            f->kind = ISA_FLOW_JUMP;
            f->target = b[oplen];
        } else {
            f->kind = ISA_FLOW_STOP;
        }
    } else if (strcmp(e->name, "JSR") == 0) {
        f->kind = ISA_FLOW_CALL;
        if (e->mode == M09_EXT)
            f->target = (int)(((unsigned)b[oplen] << 8) | b[oplen + 1]);
        else if (e->mode == M09_DIR)
            f->target = b[oplen];
    } else if (strcmp(e->name, "RTS") == 0 || strcmp(e->name, "RTI") == 0) {
        f->kind = ISA_FLOW_STOP;
    } else if ((e->mode == M09_PULS || e->mode == M09_PULU) && (b[oplen] & 0x80)) {
        f->kind = ISA_FLOW_STOP;                        /* pulls the PC: a return */
    } else if ((e->mode == M09_RR) && (b[oplen] & 15) == 5) {
        f->kind = ISA_FLOW_STOP;                        /* TFR or EXG into the PC: a jump */
    }
}

/* ------------------------------------------------------------------ */
/*  Disassembling                                                       */
/* ------------------------------------------------------------------ */
/* The text of an indexed operand; p is the postbyte and what follows it, next the
 * address of the instruction after this one.
 *
 * Author: Thomas Dzubin */
static void idx_text(char *out, size_t size, unsigned next, const uint8_t *p)
{
    uint8_t pb = p[0];
    char r = "XYUS"[(pb >> 5) & 3];
    bool ind = (pb & 0x10) != 0;
    char core[32];

    if (!(pb & 0x80)) {
        int n = pb & 0x1F;

        snprintf(core, sizeof core, "%d,%c", n & 0x10 ? n - 32 : n, r);
        ind = false;
    } else {
        switch (pb & 0x0F) {
        case 0x0:  snprintf(core, sizeof core, ",%c+", r); break;
        case 0x1:  snprintf(core, sizeof core, ",%c++", r); break;
        case 0x2:  snprintf(core, sizeof core, ",-%c", r); break;
        case 0x3:  snprintf(core, sizeof core, ",--%c", r); break;
        case 0x4:  snprintf(core, sizeof core, ",%c", r); break;
        case 0x5:  snprintf(core, sizeof core, "B,%c", r); break;
        case 0x6:  snprintf(core, sizeof core, "A,%c", r); break;
        case 0xB:  snprintf(core, sizeof core, "D,%c", r); break;
        case 0x8: {                                     /* an 8-bit offset */
            int n = (int8_t)p[1];

            snprintf(core, sizeof core, "%s%d,%c", n >= -16 && n <= 15 ? "<" : "", n, r);
            break;
        }
        case 0x9: {                                     /* a 16-bit offset */
            int n = (int16_t)(((unsigned)p[1] << 8) | p[2]);

            snprintf(core, sizeof core, "%s%d,%c", n >= -128 && n <= 127 ? ">" : "", n, r);
            break;
        }
        case 0xC:                                       /* relative to the PC, 8 bits */
            snprintf(core, sizeof core, "$%04X,PCR", (next + (unsigned)(int8_t)p[1]) & 0xFFFFu);
            break;
        case 0xD: {                                     /* ... and 16 bits */
            int n = (int16_t)(((unsigned)p[1] << 8) | p[2]);

            snprintf(core, sizeof core, "%s$%04X,PCR", n >= -128 && n <= 127 ? ">" : "",
                     (next + (unsigned)n) & 0xFFFFu);
            break;
        }
        default:                                        /* 0xF: [$1234] */
            snprintf(core, sizeof core, "$%04X", ((unsigned)p[1] << 8) | p[2]);
            break;
        }
    }
    snprintf(out, size, "%s%s%s", ind ? "[" : "", core, ind ? "]" : "");
}

/* Author: Thomas Dzubin */
static int disasm(uint16_t addr, const uint8_t *b, const char *target, char *text, size_t size)
{
    int oplen;
    const entry_t *e = decode(b, &oplen);
    int len;
    char operand[48];
    unsigned w;

    if (!e) {
        snprintf(text, size, "DB %02X", b[0]);
        return 1;
    }
    len = instruction_length(e, b, oplen);
    if (len == 0) {                                     /* data: the opcode and its postbyte */
        snprintf(text, size, "DB %02X %02X", b[0], b[1]);
        return oplen + 1;
    }
    w = ((unsigned)b[oplen] << 8) | b[oplen + 1];
    switch (e->mode) {
    case M09_INH:
        snprintf(text, size, "%s", e->name);
        return len;
    case M09_IMM8:
        snprintf(operand, sizeof operand, "#$%02X", b[oplen]);
        break;
    case M09_IMM16:
        snprintf(operand, sizeof operand, "#$%04X", w);
        break;
    case M09_DIR:
        snprintf(operand, sizeof operand, "$%02X", b[oplen]);
        break;
    case M09_EXT:
        if ((strcmp(e->name, "JMP") == 0 || strcmp(e->name, "JSR") == 0) && target)
            snprintf(operand, sizeof operand, "%s", target);
        else
            snprintf(operand, sizeof operand, "%s$%04X", w < 0x100 ? ">" : "", w);
        break;
    case M09_IDX:
        idx_text(operand, sizeof operand, (unsigned)addr + (unsigned)len, &b[oplen]);
        break;
    case M09_REL:
        if (target)
            snprintf(operand, sizeof operand, "%s", target);
        else
            snprintf(operand, sizeof operand, "$%04X",
                     ((unsigned)addr + (unsigned)len + (unsigned)(int8_t)b[oplen]) & 0xFFFFu);
        break;
    case M09_LREL:
        if (target)
            snprintf(operand, sizeof operand, "%s", target);
        else
            snprintf(operand, sizeof operand, "$%04X",
                     ((unsigned)addr + (unsigned)len + (unsigned)(int16_t)w) & 0xFFFFu);
        break;
    case M09_RR:
        snprintf(operand, sizeof operand, "%s,%s", rr_names[b[oplen] >> 4], rr_names[b[oplen] & 15]);
        break;
    default: {                                          /* PSHS, PULS, PSHU, PULU */
        size_t pos = 0;
        int bit;

        operand[0] = '\0';
        for (bit = 0; bit < 8; bit++) {
            if (!(b[oplen] & (1u << bit)))
                continue;
            pos += (size_t)snprintf(operand + pos, sizeof operand - pos, "%s%s", pos ? "," : "",
                                    bit == 6 ? (e->mode == M09_PSHU || e->mode == M09_PULU ? "S" : "U")
                                             : list_names[bit]);
        }
        if (pos == 0)
            snprintf(operand, sizeof operand, "#0");    /* an empty list */
        break;
    }
    }
    snprintf(text, size, "%-4s %s", e->name, operand);
    return len;
}

/* ------------------------------------------------------------------ */
/*  Assembling                                                          */
/* ------------------------------------------------------------------ */
static const entry_t *find(const char *name, int mode, unsigned *opcode)
{
    unsigned page, i;

    build();
    for (page = 0; page < 3; page++)
        for (i = 0; i < 256; i++)
            if (pages[page][i].name && pages[page][i].mode == mode &&
                strcmp(pages[page][i].name, name) == 0) {
                *opcode = (page == 0 ? 0u : (page == 1 ? 0x1000u : 0x1100u)) | i;
                return &pages[page][i];
            }
    return NULL;
}

static bool known_name(const char *name)
{
    unsigned page, i;

    build();
    for (page = 0; page < 3; page++)
        for (i = 0; i < 256; i++)
            if (pages[page][i].name && strcmp(pages[page][i].name, name) == 0)
                return true;
    return false;
}

static void emit_opcode(asm_t *a, unsigned opcode)
{
    if (opcode > 0xFFu)
        asm_emit(a, (uint8_t)(opcode >> 8));
    asm_emit(a, (uint8_t)(opcode & 0xFFu));
}

/* The index register named at *p (X Y U S), or -1. */
static int index_register(const char **pp)
{
    const char *p = asm_skip_space(*pp);
    char c = asm_upper(*p);
    int r = c == 'X' ? 0 : c == 'Y' ? 1 : c == 'U' ? 2 : c == 'S' ? 3 : -1;

    if (r < 0 || asm_is_name_char(p[1]))
        return -1;
    *pp = p + 1;
    return r;
}

/* Assemble an indexed operand into postbyte and offset bytes (out), for an
 * instruction at the current location whose opcode is oplen bytes long. Returns
 * the number of bytes, or 0 after noting an error.
 *
 * Author: Thomas Dzubin */
static int indexed_operand(asm_t *a, const char *p, int oplen, uint8_t *out)
{
    bool ind = false;
    char text[ASM_LINE_MAX + 2];
    size_t n;
    int r;
    uint32_t v = 0;
    char size_prefix = 0;

    p = asm_skip_space(p);
    if (*p == '[') {                                    /* indirect: strip the brackets */
        const char *close = strrchr(p, ']');

        if (!close || *asm_skip_space(close + 1) != '\0') {
            asm_error(a, ASM_ERR_MODE);
            return 0;
        }
        n = (size_t)(close - (p + 1));
        if (n >= sizeof text)
            n = sizeof text - 1;
        memcpy(text, p + 1, n);
        text[n] = '\0';
        p = text;
        ind = true;
    }
    p = asm_skip_space(p);
    if (*p == ',') {                                    /* ,R  ,R+  ,R++  ,-R  ,--R */
        int minus = 0;
        int plus = 0;

        p++;
        while (*p == '-') {
            minus++;
            p++;
        }
        r = index_register(&p);
        if (r < 0) {
            asm_error(a, ASM_ERR_MODE);
            return 0;
        }
        while (*p == '+') {
            plus++;
            p++;
        }
        if (!asm_at_end(a, p))
            return 0;
        if ((minus && plus) || minus > 2 || plus > 2 || ((minus || plus) && ind && (minus == 1 || plus == 1))) {
            asm_error(a, ASM_ERR_MODE);
            return 0;
        }
        out[0] = (uint8_t)(0x80 | (r << 5) | (ind ? 0x10 : 0));
        out[0] |= (uint8_t)(minus == 1 ? 0x02 : minus == 2 ? 0x03 : plus == 1 ? 0x00 : plus == 2 ? 0x01 : 0x04);
        return 1;
    }
    if ((asm_upper(*p) == 'A' || asm_upper(*p) == 'B' || asm_upper(*p) == 'D') &&
        *asm_skip_space(p + 1) == ',') {                /* A,R  B,R  D,R */
        char acc = asm_upper(*p);

        p = asm_skip_space(p + 1) + 1;
        r = index_register(&p);
        if (r < 0 || !asm_at_end(a, p)) {
            if (r < 0)
                asm_error(a, ASM_ERR_MODE);
            return 0;
        }
        out[0] = (uint8_t)(0x80 | (r << 5) | (ind ? 0x10 : 0) | (acc == 'A' ? 0x06 : acc == 'B' ? 0x05 : 0x0B));
        return 1;
    }
    if (*p == '<' || *p == '>') {
        size_prefix = *p;
        p++;
    }
    if (!asm_expression(a, &p, &v))
        return 0;
    p = asm_skip_space(p);
    if (*p != ',') {                                    /* [$1234]: extended indirect */
        if (!ind || size_prefix || !asm_at_end(a, p)) {
            asm_error(a, ASM_ERR_MODE);
            return 0;
        }
        out[0] = 0x9F;
        out[1] = (uint8_t)(v >> 8);
        out[2] = (uint8_t)(v & 0xFFu);
        return 3;
    }
    p++;
    {
        const char *q = asm_skip_space(p);
        bool pcr = asm_upper(q[0]) == 'P' && asm_upper(q[1]) == 'C' && asm_upper(q[2]) == 'R' &&
                   !asm_is_name_char(q[3]);
        bool known = !asm_forward(a);
        int offset;

        if (pcr) {
            p = q + 3;
            r = 0;
        } else {
            r = index_register(&p);
            if (r < 0) {
                asm_error(a, ASM_ERR_MODE);
                return 0;
            }
        }
        if (!asm_at_end(a, p))
            return 0;
        if (pcr) {                                      /* v is the address; the offset is from the next instruction */
            int short_offset = (int)v - (int)(asm_location(a) + (unsigned)oplen + 2u);
            bool use_short = size_prefix == '<' || (size_prefix == 0 && known &&
                                                     short_offset >= -128 && short_offset <= 127);

            if (use_short) {
                if (asm_pass(a) == 2 && (short_offset < -128 || short_offset > 127)) {
                    asm_error(a, ASM_ERR_RANGE);
                    return 0;
                }
                out[0] = (uint8_t)(0x8C | (ind ? 0x10 : 0));
                out[1] = (uint8_t)(short_offset & 0xFF);
                return 2;
            }
            offset = (int)v - (int)(asm_location(a) + (unsigned)oplen + 3u);
            out[0] = (uint8_t)(0x8D | (ind ? 0x10 : 0));
            out[1] = (uint8_t)((offset >> 8) & 0xFF);
            out[2] = (uint8_t)(offset & 0xFF);
            return 3;
        }
        offset = (int)(int16_t)(uint16_t)v;             /* a negative number wraps to 16 bits */
        if (size_prefix == 0 && known && !ind && offset >= -16 && offset <= 15) {
            out[0] = (uint8_t)((r << 5) | (offset & 0x1F));         /* a 5-bit offset */
            return 1;
        }
        if (size_prefix == '<' || (size_prefix == 0 && known && offset >= -128 && offset <= 127)) {
            if (offset < -128 || offset > 127) {
                asm_error(a, ASM_ERR_RANGE);
                return 0;
            }
            out[0] = (uint8_t)(0x88 | (r << 5) | (ind ? 0x10 : 0));
            out[1] = (uint8_t)(offset & 0xFF);
            return 2;
        }
        out[0] = (uint8_t)(0x89 | (r << 5) | (ind ? 0x10 : 0));
        out[1] = (uint8_t)((offset >> 8) & 0xFF);
        out[2] = (uint8_t)(offset & 0xFF);
        return 3;
    }
}

/* The code of a register name in TFR and EXG (-1 if it is not one). */
static int rr_code(const char **pp)
{
    const char *q = asm_skip_space(*pp);
    char word[4];
    int i;

    if (!asm_is_name_start(*q))
        return -1;
    asm_read_word(&q, word, sizeof word);
    for (i = 0; i < 16; i++)
        if (rr_names[i] && strcmp(rr_names[i], word) == 0) {
            *pp = q;
            return i;
        }
    return -1;
}

/* Author: Thomas Dzubin */
static bool encode(asm_t *a, const char *mnemonic, const char *operands)
{
    const char *p = asm_skip_space(operands);
    unsigned opcode = 0;
    const entry_t *e;
    uint32_t v = 0;
    uint8_t byte;
    int oplen;

    if (!known_name(mnemonic))
        return false;

    if (*p == '\0') {                                   /* inherent */
        if ((e = find(mnemonic, M09_INH, &opcode)) != NULL)
            emit_opcode(a, opcode);
        else
            asm_error(a, ASM_ERR_MISSING);
        return true;
    }
    if (*p == '#') {                                    /* immediate */
        p++;
        if ((e = find(mnemonic, M09_IMM8, &opcode)) != NULL) {
            if (asm_expression(a, &p, &v) && asm_at_end(a, p) && asm_byte_value(a, v, &byte)) {
                emit_opcode(a, opcode);
                asm_emit(a, byte);
            }
        } else if ((e = find(mnemonic, M09_IMM16, &opcode)) != NULL) {
            if (asm_expression(a, &p, &v) && asm_at_end(a, p)) {
                emit_opcode(a, opcode);
                asm_emit(a, (uint8_t)(v >> 8));
                asm_emit(a, (uint8_t)(v & 0xFFu));
            }
        } else if (strcmp(mnemonic, "PSHS") == 0 || strcmp(mnemonic, "PULS") == 0 ||
                   strcmp(mnemonic, "PSHU") == 0 || strcmp(mnemonic, "PULU") == 0) {
            if (asm_expression(a, &p, &v) && asm_at_end(a, p) && v == 0) {      /* an empty list */
                find(mnemonic, strcmp(mnemonic, "PSHS") == 0 ? M09_PSHS : strcmp(mnemonic, "PULS") == 0 ?
                     M09_PULS : strcmp(mnemonic, "PSHU") == 0 ? M09_PSHU : M09_PULU, &opcode);
                emit_opcode(a, opcode);
                asm_emit(a, 0);
            }
        } else {
            asm_error(a, ASM_ERR_MODE);
        }
        return true;
    }
    if ((e = find(mnemonic, M09_REL, &opcode)) != NULL || (e = find(mnemonic, M09_LREL, &opcode)) != NULL) {
        bool lrel = e->mode == M09_LREL;
        long offset;

        oplen = opcode > 0xFFu ? 2 : 1;
        if (asm_expression(a, &p, &v) && asm_at_end(a, p)) {
            offset = (long)v - (long)(asm_location(a) + (unsigned)oplen + (lrel ? 2u : 1u));
            if (!lrel && asm_pass(a) == 2 && (offset < -128 || offset > 127)) {
                asm_error(a, ASM_ERR_RANGE);
                return true;
            }
            emit_opcode(a, opcode);
            if (lrel)
                asm_emit(a, (uint8_t)((offset >> 8) & 0xFF));
            asm_emit(a, (uint8_t)(offset & 0xFF));
        }
        return true;
    }
    if ((e = find(mnemonic, M09_RR, &opcode)) != NULL) {            /* TFR and EXG */
        int r1 = rr_code(&p);
        int r2;
        const char *q = asm_skip_space(p);

        if (r1 < 0 || *q != ',') {
            asm_error(a, ASM_ERR_MODE);
            return true;
        }
        p = q + 1;
        r2 = rr_code(&p);
        if (r2 < 0 || !asm_at_end(a, p) || (r1 < 8) != (r2 < 8)) {
            if (r2 < 0 || (r1 < 8) != (r2 < 8))
                asm_error(a, ASM_ERR_MODE);
            return true;
        }
        emit_opcode(a, opcode);
        asm_emit(a, (uint8_t)((r1 << 4) | r2));
        return true;
    }
    {
        int mode = -1;

        if (strcmp(mnemonic, "PSHS") == 0)      mode = M09_PSHS;
        else if (strcmp(mnemonic, "PULS") == 0) mode = M09_PULS;
        else if (strcmp(mnemonic, "PSHU") == 0) mode = M09_PSHU;
        else if (strcmp(mnemonic, "PULU") == 0) mode = M09_PULU;
        if (mode >= 0) {                                /* a register list */
            uint8_t bits = 0;
            bool u_stack = mode == M09_PSHU || mode == M09_PULU;

            find(mnemonic, mode, &opcode);
            for (;;) {
                char word[4];
                int i;
                bool found = false;

                p = asm_skip_space(p);
                if (!asm_is_name_start(*p))
                    break;
                asm_read_word(&p, word, sizeof word);
                if (strcmp(word, "D") == 0) {
                    bits |= 0x06;
                    found = true;
                } else if (strcmp(word, u_stack ? "S" : "U") == 0) {
                    bits |= 0x40;
                    found = true;
                } else {
                    for (i = 0; i < 8; i++)
                        if (i != 6 && strcmp(word, list_names[i]) == 0) {
                            bits |= (uint8_t)(1u << i);
                            found = true;
                        }
                }
                if (!found) {
                    asm_error(a, ASM_ERR_MODE);
                    return true;
                }
                p = asm_skip_space(p);
                if (*p != ',')
                    break;
                p++;
            }
            if (asm_at_end(a, p)) {
                emit_opcode(a, opcode);
                asm_emit(a, bits);
            }
            return true;
        }
    }

    /* an address: direct, extended or indexed */
    {
        bool indexed = *p == '[' || *p == ',';
        const char *q;
        int depth = 0;

        for (q = p; *q && !indexed; q++) {              /* a comma outside brackets: indexed */
            if (*q == '[')
                depth++;
            else if (*q == ']')
                depth--;
            else if (*q == ',' && depth == 0)
                indexed = true;
        }
        if (indexed) {
            uint8_t bytes[3];
            int count, i;

            e = find(mnemonic, M09_IDX, &opcode);
            if (!e) {
                asm_error(a, ASM_ERR_MODE);
                return true;
            }
            oplen = opcode > 0xFFu ? 2 : 1;
            count = indexed_operand(a, p, oplen, bytes);
            if (count == 0)
                return true;
            emit_opcode(a, opcode);
            for (i = 0; i < count; i++)
                asm_emit(a, bytes[i]);
            return true;
        }
    }
    {
        char size_prefix = 0;
        unsigned dir, ext;
        bool have_dir, have_ext, use_dir;

        if (*p == '<' || *p == '>') {
            size_prefix = *p;
            p++;
        }
        if (!asm_expression(a, &p, &v) || !asm_at_end(a, p))
            return true;
        have_dir = find(mnemonic, M09_DIR, &dir) != NULL;
        have_ext = find(mnemonic, M09_EXT, &ext) != NULL;
        use_dir = have_dir && size_prefix != '>' && v < 0x100u &&
                  (size_prefix == '<' || !asm_forward(a) || !have_ext);
        if (size_prefix == '<' && (!have_dir || v >= 0x100u)) {
            asm_error(a, ASM_ERR_MODE);
            return true;
        }
        if (use_dir) {
            emit_opcode(a, dir);
            asm_emit(a, (uint8_t)v);
        } else if (have_ext) {
            emit_opcode(a, ext);
            asm_emit(a, (uint8_t)(v >> 8));
            asm_emit(a, (uint8_t)(v & 0xFFu));
        } else {
            asm_error(a, ASM_ERR_MODE);
        }
    }
    return true;
}

/* The registers cannot be labels. */
static bool reserved(const char *name)
{
    static const char *const names[] = { "A", "B", "D", "X", "Y", "U", "S", "PC", "PCR", "CC", "DP" };
    size_t i;

    for (i = 0; i < sizeof names / sizeof names[0]; i++)
        if (strcmp(name, names[i]) == 0)
            return true;
    return false;
}

const isa_t isa_6809 = {
    "6809",
    5,
    true,                                                   /* DW: high byte first */
    ISA_NUM_DOLLAR_HEX | ISA_NUM_PERCENT_BINARY | ISA_NUM_0X,
    disasm,
    flow,
    NULL,
    encode,
    reserved,
    NULL,
    NULL
};
