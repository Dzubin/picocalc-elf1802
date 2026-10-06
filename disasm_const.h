/*
 * disasm_const.h - the mnemonic tables of the 1802 disassembler (disasm1802.c),
 * in RCA's own spelling. Only disasm1802.c and the 1802's instruction-set description (isa_1802.c, for
 * the operand kinds) include this file.
 *
 * Author: Thomas Dzubin
 */
#ifndef DISASM_CONST_H
#define DISASM_CONST_H

/* What follows the opcode byte, which gives the instruction's length. */
#define DIS_NONE    0       /* nothing: one byte                              */
#define DIS_REG     1       /* the register number is the opcode's low nibble */
#define DIS_PORT    2       /* the I/O port is the low nibble (OUT) or the
                               low nibble less 8 (INP)                        */
#define DIS_IMM     3       /* a data byte follows: two bytes                 */
#define DIS_SHORT   4       /* a branch to an address on the same page: two
                               bytes                                          */
#define DIS_SKIP    5       /* a short skip: two bytes, nothing shown         */
#define DIS_LONG    6       /* a 16-bit address follows: three bytes          */

typedef struct {
    const char *name;
    int         operand;
} dis_op_t;

/* 3x: the short branches (38 is SKP, the branch that is never taken). */
#define DIS_BRANCH_TABLE { \
    { "BR",   DIS_SHORT }, { "BQ",  DIS_SHORT }, { "BZ",  DIS_SHORT }, \
    { "BDF",  DIS_SHORT }, { "B1",  DIS_SHORT }, { "B2",  DIS_SHORT }, \
    { "B3",   DIS_SHORT }, { "B4",  DIS_SHORT }, { "SKP", DIS_SKIP  }, \
    { "BNQ",  DIS_SHORT }, { "BNZ", DIS_SHORT }, { "BNF", DIS_SHORT }, \
    { "BN1",  DIS_SHORT }, { "BN2", DIS_SHORT }, { "BN3", DIS_SHORT }, \
    { "BN4",  DIS_SHORT } }

/* 7x: return, interrupt-enable, the add and subtract with carry group. */
#define DIS_SEVEN_TABLE { \
    { "RET",  DIS_NONE }, { "DIS",  DIS_NONE }, { "LDXA", DIS_NONE }, \
    { "STXD", DIS_NONE }, { "ADC",  DIS_NONE }, { "SDB",  DIS_NONE }, \
    { "SHRC", DIS_NONE }, { "SMB",  DIS_NONE }, { "SAV",  DIS_NONE }, \
    { "MARK", DIS_NONE }, { "REQ",  DIS_NONE }, { "SEQ",  DIS_NONE }, \
    { "ADCI", DIS_IMM  }, { "SDBI", DIS_IMM  }, { "SHLC", DIS_NONE }, \
    { "SMBI", DIS_IMM  } }

/* Cx: long branches, NOP and the long skips. */
#define DIS_LONG_TABLE { \
    { "LBR",  DIS_LONG }, { "LBQ",  DIS_LONG }, { "LBZ",  DIS_LONG }, \
    { "LBDF", DIS_LONG }, { "NOP",  DIS_NONE }, { "LSNQ", DIS_NONE }, \
    { "LSNZ", DIS_NONE }, { "LSNF", DIS_NONE }, { "LSKP", DIS_NONE }, \
    { "LBNQ", DIS_LONG }, { "LBNZ", DIS_LONG }, { "LBNF", DIS_LONG }, \
    { "LSIE", DIS_NONE }, { "LSQ",  DIS_NONE }, { "LSZ",  DIS_NONE }, \
    { "LSDF", DIS_NONE } }

/* Fx: the arithmetic and logic group, with D and M(R(X)) or an immediate. */
#define DIS_ALU_TABLE { \
    { "LDX",  DIS_NONE }, { "OR",   DIS_NONE }, { "AND",  DIS_NONE }, \
    { "XOR",  DIS_NONE }, { "ADD",  DIS_NONE }, { "SD",   DIS_NONE }, \
    { "SHR",  DIS_NONE }, { "SM",   DIS_NONE }, { "LDI",  DIS_IMM  }, \
    { "ORI",  DIS_IMM  }, { "ANI",  DIS_IMM  }, { "XRI",  DIS_IMM  }, \
    { "ADI",  DIS_IMM  }, { "SDI",  DIS_IMM  }, { "SHL",  DIS_NONE }, \
    { "SMI",  DIS_IMM  } }

/* The instructions that name a register, by the opcode's high nibble (a null
 * is a group that is something else). 0x with a low nibble of 0 is IDL. */
#define DIS_REG_NAMES { \
    "LDN", "INC", "DEC", 0, "LDA", "STR", 0, 0, \
    "GLO", "GHI", "PLO", "PHI", 0, "SEP", "SEX", 0 }

#endif /* DISASM_CONST_H */
