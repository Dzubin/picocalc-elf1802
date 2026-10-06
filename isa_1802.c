/*
 * isa_1802.c - the CDP1802's instruction set for the tools: how to write an
 * instruction, which way the program goes after it, how to turn assembly text
 * into bytes, and the debugger's read-out of the registers. The instruction
 * names come from the disassembler's tables (disasm1802.c), so what the debugger
 * prints can be typed back in.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "asm_core.h"
#include "cpu1802.h"
#include "disasm1802.h"
#include "disasm_const.h"                   /* the operand kinds (DIS_*)       */
#include "isa.h"
#include "isa_1802_const.h"

/* ------------------------------------------------------------------ */
/*  The instruction table, built from the disassembler's                */
/* ------------------------------------------------------------------ */
typedef struct {
    char    name[ISA1802_MNEMONIC_MAX + 1];
    int     kind;                           /* DIS_NONE, DIS_REG, ...          */
    uint8_t base;                           /* the opcode, less its register   */
} mnemonic_t;

static mnemonic_t mnemonics[ISA1802_MAX_MNEMONICS];
static int        mnemonic_count;

static const mnemonic_t *find_mnemonic(const char *name)
{
    int i;

    for (i = 0; i < mnemonic_count; i++)
        if (strcmp(mnemonics[i].name, name) == 0)
            return &mnemonics[i];
    return NULL;
}

static void add_mnemonic(const char *name, int kind, uint8_t base)
{
    if (mnemonic_count >= ISA1802_MAX_MNEMONICS || find_mnemonic(name))
        return;
    strncpy(mnemonics[mnemonic_count].name, name, ISA1802_MNEMONIC_MAX);
    mnemonics[mnemonic_count].name[ISA1802_MNEMONIC_MAX] = '\0';
    mnemonics[mnemonic_count].kind = kind;
    mnemonics[mnemonic_count].base = base;
    mnemonic_count++;
}

/* Author: Thomas Dzubin */
static void build_table(void)
{
    static const char *const aliases[][2] = ISA1802_ALIASES;
    unsigned op, i;

    if (mnemonic_count > 0)
        return;
    for (op = 0; op < 256; op++) {
        int kind;
        unsigned reg;
        const char *name;

        if (op == 0x68)
            continue;                       /* not an 1802 instruction (INP 0)  */
        name = dis1802_opcode((uint8_t)op, &kind, &reg);
        add_mnemonic(name, kind, (uint8_t)(op - ((kind == DIS_REG || kind == DIS_PORT) ? reg : 0)));
    }
    for (i = 0; i < sizeof aliases / sizeof aliases[0]; i++) {
        const mnemonic_t *m = find_mnemonic(aliases[i][1]);

        if (m)
            add_mnemonic(aliases[i][0], m->kind, m->base);
    }
}

/* ------------------------------------------------------------------ */
/*  Assembling                                                          */
/* ------------------------------------------------------------------ */
/* A register or port: R5, 5, or an expression.
 *
 * Author: Thomas Dzubin */
static bool register_operand(asm_t *a, const char **pp, unsigned lo, unsigned hi,
                             unsigned *reg)
{
    const char *p = asm_skip_space(*pp);
    uint32_t v;

    if (*p == '\0') {
        asm_error(a, ASM_ERR_MISSING);
        return false;
    }
    if (asm_upper(*p) == 'R' && asm_hex_digit(p[1]) >= 0 && !asm_is_name_char(p[2])) {
        v = (uint32_t)asm_hex_digit(p[1]);
        p += 2;
    } else if (!asm_expression(a, &p, &v)) {
        return false;
    }
    if (v < lo || v > hi) {
        asm_error(a, hi == 15 ? ASM_ERR_REGISTER : ASM_ERR_PORT);
        return false;
    }
    *reg = (unsigned)v;
    *pp = p;
    return true;
}

/* Author: Thomas Dzubin */
static void assemble_instruction(asm_t *a, const mnemonic_t *m, const char *p)
{
    uint32_t v;
    unsigned reg;
    uint8_t b;

    switch (m->kind) {
    case DIS_NONE:
        if (asm_at_end(a, p))
            asm_emit(a, m->base);
        break;
    case DIS_SKIP:                                      /* SKP: skips a byte   */
        if (asm_at_end(a, p)) {
            asm_emit(a, m->base);
            asm_emit(a, 0);
        }
        break;
    case DIS_REG:
        if (!register_operand(a, &p, 0, 15, &reg) || !asm_at_end(a, p))
            break;
        if (m->base == 0 && reg == 0) {
            asm_error(a, ASM_ERR_LDN0);
            break;
        }
        asm_emit(a, (uint8_t)(m->base | reg));
        break;
    case DIS_PORT:
        if (!register_operand(a, &p, 1, 7, &reg) || !asm_at_end(a, p))
            break;
        asm_emit(a, (uint8_t)(m->base | reg));
        break;
    case DIS_IMM:
        if (*asm_skip_space(p) == '\0') {
            asm_error(a, ASM_ERR_MISSING);
            break;
        }
        if (!asm_expression(a, &p, &v) || !asm_at_end(a, p) || !asm_byte_value(a, v, &b))
            break;
        asm_emit(a, m->base);
        asm_emit(a, b);
        break;
    case DIS_SHORT:
        if (*asm_skip_space(p) == '\0') {
            asm_error(a, ASM_ERR_MISSING);
            break;
        }
        if (!asm_expression(a, &p, &v) || !asm_at_end(a, p))
            break;
        if (asm_pass(a) == 2 && (v & 0xFF00u) != ((asm_location(a) + 1u) & 0xFF00u)) {
            asm_error(a, ASM_ERR_PAGE);
            break;
        }
        asm_emit(a, m->base);
        asm_emit(a, (uint8_t)(v & 0xFFu));
        break;
    case DIS_LONG:
        if (*asm_skip_space(p) == '\0') {
            asm_error(a, ASM_ERR_MISSING);
            break;
        }
        if (!asm_expression(a, &p, &v) || !asm_at_end(a, p))
            break;
        asm_emit(a, m->base);
        asm_emit(a, (uint8_t)(v >> 8));
        asm_emit(a, (uint8_t)(v & 0xFFu));
        break;
    default:
        asm_error(a, ASM_ERR_OPERAND);
        break;
    }
}

static bool encode(asm_t *a, const char *mnemonic, const char *operands)
{
    const mnemonic_t *m;

    build_table();
    m = find_mnemonic(mnemonic);
    if (!m)
        return false;
    assemble_instruction(a, m, operands);
    return true;
}

/* A register name (R0 to RF) cannot be a label. */
static bool reserved(const char *name)
{
    return name[0] == 'R' && asm_hex_digit(name[1]) >= 0 && name[2] == '\0';
}

static const char *note(const uint8_t *b)
{
    static const struct { uint8_t opcode; const char *note; } notes[] = ISA1802_NOTES;
    size_t i;

    for (i = 0; i < sizeof notes / sizeof notes[0]; i++)
        if (notes[i].opcode == b[0])
            return notes[i].note;
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Disassembling and the flow of a program                             */
/* ------------------------------------------------------------------ */
static int disasm(uint16_t addr, const uint8_t *b, const char *target, char *text,
                  size_t size)
{
    int length;
    int where = dis1802_target(addr, b, &length);

    if (target && where >= 0) {
        int kind;
        unsigned reg;
        const char *name = dis1802_opcode(b[0], &kind, &reg);

        snprintf(text, size, "%-4s %s", name, target);
        return length;
    }
    return dis1802(addr, b, text, size);
}

/* Author: Thomas Dzubin */
static void flow(uint16_t addr, const uint8_t *b, isa_flow_t *f)
{
    uint8_t op = b[0];

    f->target = dis1802_target(addr, b, &f->length);
    f->kind = ISA_FLOW_NEXT;
    f->skip = 0;
    if (op == 0x68) {                                   /* not an instruction */
        f->length = 1;
        f->kind = ISA_FLOW_BAD;
    } else if (op == 0x00 || op == 0x70 || op == 0x71) {    /* IDL, RET, DIS */
        f->length = 1;
        f->kind = ISA_FLOW_STOP;
    } else if (op == 0x38) {                            /* SKP: it and the byte it skips */
        f->length = 2;
        f->kind = ISA_FLOW_DATA;
    } else if (op == 0x30 || op == 0xC0) {              /* BR, LBR */
        f->kind = ISA_FLOW_JUMP;
    } else if ((op >= 0x31 && op <= 0x3F) || op == 0xC1 || op == 0xC2 || op == 0xC3 ||
               op == 0xC9 || op == 0xCA || op == 0xCB) {
        f->kind = ISA_FLOW_BRANCH;
    } else if (op == 0xC8) {                            /* LSKP: always skips two bytes */
        f->kind = ISA_FLOW_SKIP;
        f->skip = 2;
    } else if ((op >= 0xC5 && op <= 0xC7) || (op >= 0xCC && op <= 0xCF)) {
        f->skip = 2;                                    /* a long skip */
    }
}

/* ------------------------------------------------------------------ */
/*  Code that only a register points at                                 */
/* ------------------------------------------------------------------ */
/* The addresses that registers were loaded with (LDI then PLO, and LDI then PHI;
 * a register whose high byte was not set from a known value is taken to be on the
 * page of the instruction that set its low byte), for the registers that a SEP
 * uses and for R1, the interrupt's: code that a SEP or an interrupt goes to is
 * not named by any branch. */
typedef struct {
    bool     d_known;
    uint8_t  d;
    uint16_t lo_known, hi_known;
    uint8_t  lo[16], hi[16];
} flow_t;

/* Author: Thomas Dzubin */
static int indirect_entries(const uint8_t *image, uint32_t size, uint32_t limit,
                            const uint8_t *starts, uint16_t *out, int max)
{
    uint16_t cand[16][ISA1802_CAND_MAX];
    uint8_t ncand[16];
    uint16_t sep_used = 0;
    flow_t f;
    uint32_t pc = 0;
    int count = 0;
    unsigned r, i;

    memset(ncand, 0, sizeof ncand);
    memset(&f, 0, sizeof f);
    while (pc < limit) {
        uint8_t b[ISA_MAX_BYTES];
        isa_flow_t fl;
        bool keeps_d;
        uint8_t op;
        int k;

        if (!((starts[pc >> 3] >> (pc & 7)) & 1u)) {    /* a gap: the trace starts again */
            memset(&f, 0, sizeof f);
            pc++;
            continue;
        }
        for (k = 0; k < ISA_MAX_BYTES; k++)
            b[k] = pc + (uint32_t)k < size ? image[pc + (uint32_t)k] : 0;
        op = b[0];
        flow((uint16_t)pc, b, &fl);
        if ((op & 0xF0) == 0xD0)
            sep_used |= (uint16_t)(1u << (op & 0x0F));

        keeps_d = op == 0xF8 || (op & 0xF0) == 0xA0 || (op & 0xF0) == 0xB0 ||
                  (op & 0xF0) == 0xE0 || (op >= 0x10 && op <= 0x2F) || op == 0x7A ||
                  op == 0x7B || op == 0xC4 || (op >= 0x50 && op <= 0x5F) ||
                  (op >= 0x61 && op <= 0x67) || (op & 0xF0) == 0xD0;
        if (op == 0xF8) {
            f.d_known = true;
            f.d = b[1];
        } else if ((op & 0xF0) == 0xA0 && f.d_known) {      /* PLO: the low byte of a register */
            unsigned reg_n = op & 0x0Fu;
            uint32_t address = (f.hi_known >> reg_n) & 1u ? ((uint32_t)f.hi[reg_n] << 8) | f.d
                                                         : (pc & 0xFF00u) | f.d;
            unsigned j;
            bool have = false;

            f.lo_known |= (uint16_t)(1u << reg_n);
            f.lo[reg_n] = f.d;
            for (j = 0; j < ncand[reg_n]; j++)
                have = have || cand[reg_n][j] == address;
            if (!have && ncand[reg_n] < ISA1802_CAND_MAX)
                cand[reg_n][ncand[reg_n]++] = (uint16_t)address;
        } else if ((op & 0xF0) == 0xB0 && f.d_known) {      /* PHI: the high byte */
            unsigned reg_n = op & 0x0Fu;

            f.hi_known |= (uint16_t)(1u << reg_n);
            f.hi[reg_n] = f.d;
            if ((f.lo_known >> reg_n) & 1u) {
                uint32_t address = ((uint32_t)f.d << 8) | f.lo[reg_n];
                unsigned j;
                bool have = false;

                for (j = 0; j < ncand[reg_n]; j++)
                    have = have || cand[reg_n][j] == address;
                if (!have && ncand[reg_n] < ISA1802_CAND_MAX)
                    cand[reg_n][ncand[reg_n]++] = (uint16_t)address;
            }
        }
        if (!keeps_d)
            f.d_known = false;
        if (fl.kind == ISA_FLOW_JUMP || fl.kind == ISA_FLOW_STOP || fl.kind == ISA_FLOW_BAD ||
            fl.kind == ISA_FLOW_SKIP)
            memset(&f, 0, sizeof f);                        /* the next code is another run */
        pc += (uint32_t)fl.length;
    }
    for (r = 0; r < 16; r++) {
        if (!((sep_used >> r) & 1u) && r != 1)
            continue;
        for (i = 0; i < ncand[r]; i++) {
            uint32_t a = cand[r][i];

            if (a < limit && !((starts[a >> 3] >> (a & 7)) & 1u) && count < max)
                out[count++] = (uint16_t)a;
        }
    }
    return count;
}

/* ------------------------------------------------------------------ */
/*  The debugger's read-out                                             */
/* ------------------------------------------------------------------ */
/* Author: Thomas Dzubin */
static bool register_row(const void *regs, int row, char *text, size_t size)
{
    const cpu1802_t *c = regs;

    if (row == 0) {
        snprintf(text, size, "P:%X X:%X D:%02X DF:%d Q:%d IE:%d T:%02X",
                 c->p, c->x, c->d, c->df ? 1 : 0, c->q ? 1 : 0, c->ie ? 1 : 0, c->t);
        return true;
    }
    if (row >= 1 && row < ISA1802_REG_ROWS) {
        size_t pos = 0;
        int i;

        text[0] = '\0';
        for (i = 0; i < 4; i++) {
            int r = (row - 1) * 4 + i;
            char mark = (r == c->p && r == c->x) ? '#' :
                        (r == c->p) ? '*' : (r == c->x) ? '+' : ':';

            pos += (size_t)snprintf(text + pos, size - pos, "%sR%X%c%04X",
                                    i ? " " : "", r, mark, c->r[r]);
        }
        return true;
    }
    return false;
}

const isa_t isa_1802 = {
    "1802",
    3,
    true,                                                  /* DW: high byte first */
    ISA_NUM_HEX_DEFAULT | ISA_NUM_HASH_DECIMAL | ISA_NUM_DOLLAR_HEX | ISA_NUM_H_SUFFIX |
        ISA_NUM_0X,
    disasm,
    flow,
    indirect_entries,
    encode,
    reserved,
    note,
    register_row
};
