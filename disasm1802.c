/*
 * disasm1802.c - the 1802 disassembler. See disasm1802.h.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>

#include "disasm1802.h"
#include "disasm_const.h"

/* The opcode's name and what follows it.
 *
 * Author: Thomas Dzubin */
static dis_op_t lookup(uint8_t op, unsigned *reg)
{
    static const dis_op_t branch[16] = DIS_BRANCH_TABLE;
    static const dis_op_t seven[16]  = DIS_SEVEN_TABLE;
    static const dis_op_t longs[16]  = DIS_LONG_TABLE;
    static const dis_op_t alu[16]    = DIS_ALU_TABLE;
    static const char *const reg_names[16] = DIS_REG_NAMES;
    unsigned hi = op >> 4;
    unsigned lo = op & 15;
    dis_op_t r;

    *reg = lo;
    if (op == 0x00) {
        r.name = "IDL";
        r.operand = DIS_NONE;
    } else if (reg_names[hi]) {
        r.name = reg_names[hi];
        r.operand = DIS_REG;
    } else if (hi == 0x3) {
        r = branch[lo];
    } else if (hi == 0x6) {
        if (lo == 0) {
            r.name = "IRX";
            r.operand = DIS_NONE;
        } else if (lo < 8) {
            r.name = "OUT";
            r.operand = DIS_PORT;
        } else {
            r.name = "INP";
            r.operand = DIS_PORT;
            *reg = lo - 8;
        }
    } else if (hi == 0x7) {
        r = seven[lo];
    } else if (hi == 0xC) {
        r = longs[lo];
    } else {
        r = alu[lo];                    /* the only group left is Fx */
    }
    return r;
}

const char *dis1802_opcode(uint8_t op, int *operand, unsigned *reg)
{
    dis_op_t d = lookup(op, reg);

    *operand = d.operand;
    return d.name;
}

/* Author: Thomas Dzubin */
int dis1802_target(uint16_t addr, const uint8_t b[3], int *length)
{
    unsigned reg;
    dis_op_t op = lookup(b[0], &reg);

    switch (op.operand) {
    case DIS_SHORT:
        *length = 2;
        return (int)(((addr + 1u) & 0xFF00u) | b[1]);
    case DIS_LONG:
        *length = 3;
        return (int)(((unsigned)b[1] << 8) | b[2]);
    case DIS_IMM:
    case DIS_SKIP:
        *length = 2;
        return -1;
    default:
        *length = 1;
        return -1;
    }
}

/* Author: Thomas Dzubin */
void dis1802_mark_targets(const uint8_t *image, uint32_t size, uint8_t *marks)
{
    uint32_t limit = size;
    uint32_t addr = 0;
    uint32_t i;

    for (i = 0; i < size / 8; i++)
        marks[i] = 0;
    while (limit > 0 && image[limit - 1] == 0)
        limit--;                                /* nothing past the last byte used */
    while (addr < limit) {
        uint8_t b[3];
        int length, target;

        for (i = 0; i < 3; i++)
            b[i] = addr + i < size ? image[addr + i] : 0;
        target = dis1802_target((uint16_t)addr, b, &length);
        if (target >= 0 && (uint32_t)target < size)
            marks[target >> 3] |= (uint8_t)(1u << (target & 7));
        addr += (uint32_t)length;
    }
}

/* Author: Thomas Dzubin */
int dis1802(uint16_t addr, const uint8_t b[3], char *text, size_t size)
{
    unsigned reg;
    dis_op_t op = lookup(b[0], &reg);
    int length = 1;

    switch (op.operand) {
    case DIS_REG:
    case DIS_PORT:
        snprintf(text, size, "%-4s %X", op.name, reg);
        break;
    case DIS_IMM:
        length = 2;
        snprintf(text, size, "%-4s %02X", op.name, b[1]);
        break;
    case DIS_SHORT:
        length = 2;
        snprintf(text, size, "%-4s %04X", op.name,
                 (unsigned)(((addr + 1u) & 0xFF00u) | b[1]));
        break;
    case DIS_SKIP:
        length = 2;
        snprintf(text, size, "%s", op.name);
        break;
    case DIS_LONG:
        length = 3;
        snprintf(text, size, "%-4s %02X%02X", op.name, b[1], b[2]);
        break;
    default:
        snprintf(text, size, "%s", op.name);
        break;
    }
    return length;
}
