/*
 * isa_z80_const.h - the name tables of the Zilog Z80's description (isa_z80.c).
 * The Z80 builds its opcodes from bit fields (x = bits 7-6, y = bits 5-3,
 * z = bits 2-0, and y split in p = y / 2 and q = y mod 2), so the description
 * decodes by those fields and looks the names up here, instead of listing every
 * opcode. Documented instructions only: the undocumented ones (SLL, the halves of
 * IX and IY, IN F,(C) and the like) are not instructions to the tools.
 *
 * Author: Thomas Dzubin
 */
#ifndef ISA_Z80_CONST_H
#define ISA_Z80_CONST_H

/* The prefix bytes. CB is the rotate, shift and bit group; ED the extended group
 * (block moves, 16-bit arithmetic with carry, I/O through C); DD and FD put IX
 * or IY in the place of HL. DD CB d op and FD CB d op work on (IX+d) and (IY+d). */
#define Z80_PREFIX_CB   0xCB
#define Z80_PREFIX_ED   0xED
#define Z80_PREFIX_DD   0xDD
#define Z80_PREFIX_FD   0xFD

/* The opcode tables the assembler searches: which prefix bytes come before the
 * opcode. */
#define Z80_TABLE_BASE  0
#define Z80_TABLE_CB    1
#define Z80_TABLE_ED    2
#define Z80_TABLE_DD    3
#define Z80_TABLE_FD    4
#define Z80_TABLE_DDCB  5
#define Z80_TABLE_FDCB  6
#define Z80_TABLE_COUNT 7

/* The registers and pairs by their number in the opcode. Register 6 is the byte
 * HL points at. */
#define ISAZ80_REGS     { "B", "C", "D", "E", "H", "L", "(HL)", "A" }
#define ISAZ80_PAIRS    { "BC", "DE", "HL", "SP" }
#define ISAZ80_PAIRS_AF { "BC", "DE", "HL", "AF" }
#define ISAZ80_CONDS    { "NZ", "Z", "NC", "C", "PO", "PE", "P", "M" }

/* The arithmetic group (the first operand of ADD, ADC and SBC is the accumulator),
 * the rotates and shifts, the accumulator group, the bit numbers, the restart
 * addresses and the block instructions (by z, then y - 4). */
#define ISAZ80_ALU      { "ADD", "ADC", "SUB", "SBC", "AND", "XOR", "OR", "CP" }
#define ISAZ80_ALU_WITH_A   0x0Bu       /* bit n set: ALU n writes "A," (ADD, ADC, SBC) */
#define ISAZ80_ROT      { "RLC", "RRC", "RL", "RR", "SLA", "SRA", "SLL", "SRL" }
#define ISAZ80_ROT_SLL  6               /* the undocumented one */
#define ISAZ80_ACC      { "RLCA", "RRCA", "RLA", "RRA", "DAA", "CPL", "SCF", "CCF" }
#define ISAZ80_BITS     { "0", "1", "2", "3", "4", "5", "6", "7" }
#define ISAZ80_RST      { "00H", "08H", "10H", "18H", "20H", "28H", "30H", "38H" }
#define ISAZ80_BLOCK { \
    { "LDI",  "LDD",  "LDIR", "LDDR" }, \
    { "CPI",  "CPD",  "CPIR", "CPDR" }, \
    { "INI",  "IND",  "INIR", "INDR" }, \
    { "OUTI", "OUTD", "OTIR", "OTDR" } }

/* The words that are registers or conditions: a label cannot be one of them, and
 * an operand that starts with one is that register, not a number. */
#define ISAZ80_RESERVED { "A", "B", "C", "D", "E", "H", "L", "I", "R", "F", "AF", "BC", "DE", \
                          "HL", "SP", "IX", "IY", "NZ", "Z", "NC", "PO", "PE", "P", "M" }
#define ISAZ80_RESERVED_MAX 2           /* the longest of them, in characters */

/* Operand texts have these placeholders in them, in the order the bytes follow the
 * opcode: n (a byte), nn (a word), e (a relative branch) and +d (the signed
 * displacement of (IX+d) and (IY+d)). Everything else in an operand text is
 * written as it is. */
#define Z80_OPERAND_MAX     32          /* characters of an operand text, expanded */
#define Z80_PLACEHOLDERS    3           /* most in one instruction ((IX+d),n has two) */
#define Z80_TEMPLATE_MAX    40          /* characters of both operands, with the comma */

#endif /* ISA_Z80_CONST_H */
