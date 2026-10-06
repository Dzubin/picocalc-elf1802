/*
 * isa_6800_const.h - the instruction tables of the Motorola 6800's and 6803's
 * description (isa_6800.c). The 6803 (and 6801) is the 6800 with a few more
 * instructions; each entry says which of the two first has it.
 *
 * The accumulator instructions come in a grid: the same eleven operations for the
 * A and the B accumulator, each in four addressing modes, so they are described as
 * a family and not as 176 entries.
 *
 * Author: Thomas Dzubin
 */
#ifndef ISA_6800_CONST_H
#define ISA_6800_CONST_H

/* Addressing modes. */
#define M68_INH     0       /* inherent:            RTS                 */
#define M68_IMM8    1       /* immediate byte:      LDAA #$12           */
#define M68_IMM16   2       /* immediate word:      LDX #$1234          */
#define M68_DIR     3       /* direct:              LDAA $12            */
#define M68_IDX     4       /* indexed:             LDAA 5,X            */
#define M68_EXT     5       /* extended:            LDAA $1234          */
#define M68_REL     6       /* relative branch:     BNE $1234           */

/* Which processor first has an instruction. */
#define L6800       0
#define L6803       1

/* { opcode, mnemonic, mode, first processor }: everything that is not in the grid
 * of accumulator instructions or the family of memory instructions. */
#define ISA6800_OPS { \
    { 0x01, "NOP",  M68_INH, L6800 }, { 0x04, "LSRD", M68_INH, L6803 }, { 0x05, "ASLD", M68_INH, L6803 }, \
    { 0x06, "TAP",  M68_INH, L6800 }, { 0x07, "TPA",  M68_INH, L6800 }, { 0x08, "INX",  M68_INH, L6800 }, \
    { 0x09, "DEX",  M68_INH, L6800 }, { 0x0A, "CLV",  M68_INH, L6800 }, { 0x0B, "SEV",  M68_INH, L6800 }, \
    { 0x0C, "CLC",  M68_INH, L6800 }, { 0x0D, "SEC",  M68_INH, L6800 }, { 0x0E, "CLI",  M68_INH, L6800 }, \
    { 0x0F, "SEI",  M68_INH, L6800 }, \
    { 0x10, "SBA",  M68_INH, L6800 }, { 0x11, "CBA",  M68_INH, L6800 }, { 0x16, "TAB",  M68_INH, L6800 }, \
    { 0x17, "TBA",  M68_INH, L6800 }, { 0x19, "DAA",  M68_INH, L6800 }, { 0x1B, "ABA",  M68_INH, L6800 }, \
    { 0x20, "BRA",  M68_REL, L6800 }, { 0x21, "BRN",  M68_REL, L6803 }, { 0x22, "BHI",  M68_REL, L6800 }, \
    { 0x23, "BLS",  M68_REL, L6800 }, { 0x24, "BCC",  M68_REL, L6800 }, { 0x25, "BCS",  M68_REL, L6800 }, \
    { 0x26, "BNE",  M68_REL, L6800 }, { 0x27, "BEQ",  M68_REL, L6800 }, { 0x28, "BVC",  M68_REL, L6800 }, \
    { 0x29, "BVS",  M68_REL, L6800 }, { 0x2A, "BPL",  M68_REL, L6800 }, { 0x2B, "BMI",  M68_REL, L6800 }, \
    { 0x2C, "BGE",  M68_REL, L6800 }, { 0x2D, "BLT",  M68_REL, L6800 }, { 0x2E, "BGT",  M68_REL, L6800 }, \
    { 0x2F, "BLE",  M68_REL, L6800 }, \
    { 0x30, "TSX",  M68_INH, L6800 }, { 0x31, "INS",  M68_INH, L6800 }, { 0x32, "PULA", M68_INH, L6800 }, \
    { 0x33, "PULB", M68_INH, L6800 }, { 0x34, "DES",  M68_INH, L6800 }, { 0x35, "TXS",  M68_INH, L6800 }, \
    { 0x36, "PSHA", M68_INH, L6800 }, { 0x37, "PSHB", M68_INH, L6800 }, { 0x38, "PULX", M68_INH, L6803 }, \
    { 0x39, "RTS",  M68_INH, L6800 }, { 0x3A, "ABX",  M68_INH, L6803 }, { 0x3B, "RTI",  M68_INH, L6800 }, \
    { 0x3C, "PSHX", M68_INH, L6803 }, { 0x3D, "MUL",  M68_INH, L6803 }, { 0x3E, "WAI",  M68_INH, L6800 }, \
    { 0x3F, "SWI",  M68_INH, L6800 }, \
    { 0x40, "NEGA", M68_INH, L6800 }, { 0x43, "COMA", M68_INH, L6800 }, { 0x44, "LSRA", M68_INH, L6800 }, \
    { 0x46, "RORA", M68_INH, L6800 }, { 0x47, "ASRA", M68_INH, L6800 }, { 0x48, "ASLA", M68_INH, L6800 }, \
    { 0x49, "ROLA", M68_INH, L6800 }, { 0x4A, "DECA", M68_INH, L6800 }, { 0x4C, "INCA", M68_INH, L6800 }, \
    { 0x4D, "TSTA", M68_INH, L6800 }, { 0x4F, "CLRA", M68_INH, L6800 }, \
    { 0x50, "NEGB", M68_INH, L6800 }, { 0x53, "COMB", M68_INH, L6800 }, { 0x54, "LSRB", M68_INH, L6800 }, \
    { 0x56, "RORB", M68_INH, L6800 }, { 0x57, "ASRB", M68_INH, L6800 }, { 0x58, "ASLB", M68_INH, L6800 }, \
    { 0x59, "ROLB", M68_INH, L6800 }, { 0x5A, "DECB", M68_INH, L6800 }, { 0x5C, "INCB", M68_INH, L6800 }, \
    { 0x5D, "TSTB", M68_INH, L6800 }, { 0x5F, "CLRB", M68_INH, L6800 }, \
    { 0x8C, "CPX",  M68_IMM16, L6800 }, { 0x8D, "BSR",  M68_REL, L6800 }, { 0x8E, "LDS",  M68_IMM16, L6800 }, \
    { 0x9C, "CPX",  M68_DIR, L6800 }, { 0x9D, "JSR",  M68_DIR, L6803 }, { 0x9E, "LDS",  M68_DIR, L6800 }, \
    { 0x9F, "STS",  M68_DIR, L6800 }, \
    { 0xAC, "CPX",  M68_IDX, L6800 }, { 0xAD, "JSR",  M68_IDX, L6800 }, { 0xAE, "LDS",  M68_IDX, L6800 }, \
    { 0xAF, "STS",  M68_IDX, L6800 }, \
    { 0xBC, "CPX",  M68_EXT, L6800 }, { 0xBD, "JSR",  M68_EXT, L6800 }, { 0xBE, "LDS",  M68_EXT, L6800 }, \
    { 0xBF, "STS",  M68_EXT, L6800 }, \
    { 0xCE, "LDX",  M68_IMM16, L6800 }, { 0xDE, "LDX",  M68_DIR, L6800 }, { 0xDF, "STX",  M68_DIR, L6800 }, \
    { 0xEE, "LDX",  M68_IDX, L6800 }, { 0xEF, "STX",  M68_IDX, L6800 }, { 0xFE, "LDX",  M68_EXT, L6800 }, \
    { 0xFF, "STX",  M68_EXT, L6800 }, \
    { 0x83, "SUBD", M68_IMM16, L6803 }, { 0x93, "SUBD", M68_DIR, L6803 }, { 0xA3, "SUBD", M68_IDX, L6803 }, \
    { 0xB3, "SUBD", M68_EXT, L6803 }, { 0xC3, "ADDD", M68_IMM16, L6803 }, { 0xD3, "ADDD", M68_DIR, L6803 }, \
    { 0xE3, "ADDD", M68_IDX, L6803 }, { 0xF3, "ADDD", M68_EXT, L6803 }, { 0xCC, "LDD",  M68_IMM16, L6803 }, \
    { 0xDC, "LDD",  M68_DIR, L6803 }, { 0xEC, "LDD",  M68_IDX, L6803 }, { 0xFC, "LDD",  M68_EXT, L6803 }, \
    { 0xDD, "STD",  M68_DIR, L6803 }, { 0xED, "STD",  M68_IDX, L6803 }, { 0xFD, "STD",  M68_EXT, L6803 } }

/* The memory instructions: opcodes 60 to 6F (indexed) and 70 to 7F (extended),
 * { low nibble, mnemonic }. */
#define ISA6800_MEMORY { \
    { 0x0, "NEG" }, { 0x3, "COM" }, { 0x4, "LSR" }, { 0x6, "ROR" }, { 0x7, "ASR" }, \
    { 0x8, "ASL" }, { 0x9, "ROL" }, { 0xA, "DEC" }, { 0xC, "INC" }, { 0xD, "TST" }, \
    { 0xE, "JMP" }, { 0xF, "CLR" } }

/* The accumulator grid: the opcodes 80 to BF for A and C0 to FF for B, in the
 * immediate, direct, indexed and extended columns; { low nibble, base mnemonic,
 * whether it has an immediate form }. */
#define ISA6800_ACCUM { \
    { 0x0, "SUB", 1 }, { 0x1, "CMP", 1 }, { 0x2, "SBC", 1 }, { 0x4, "AND", 1 }, \
    { 0x5, "BIT", 1 }, { 0x6, "LDA", 1 }, { 0x7, "STA", 0 }, { 0x8, "EOR", 1 }, \
    { 0x9, "ADC", 1 }, { 0xA, "ORA", 1 }, { 0xB, "ADD", 1 } }

#endif /* ISA_6800_CONST_H */
