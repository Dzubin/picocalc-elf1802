/*
 * isa_8080_const.h - the instruction table of the Intel 8080's description
 * (isa_8080.c). The 8080 packs its register choices into the opcode, so the table
 * lists a base opcode and what varies in it, not 256 separate entries.
 *
 * Author: Thomas Dzubin
 */
#ifndef ISA_8080_CONST_H
#define ISA_8080_CONST_H

/* What an entry's operands are, and which bits of the opcode they use. */
#define K80_NONE    0       /* no operands:                      NOP            */
#define K80_R3      1       /* a register in bits 0-2:           ADD B          */
#define K80_R3D     2       /* a register in bits 3-5:           INR B          */
#define K80_MOV     3       /* two registers, bits 3-5 and 0-2:  MOV A,B        */
#define K80_MVI     4       /* a register (bits 3-5), a byte:    MVI A,12H      */
#define K80_RP      5       /* a register pair BC DE HL SP (bits 4-5): INX H    */
#define K80_LXI     6       /* a register pair and a word:       LXI H,1234H    */
#define K80_STAX    7       /* the pair BC or DE (bit 4):        STAX B         */
#define K80_RPP     8       /* a pair BC DE HL PSW (bits 4-5):   PUSH PSW       */
#define K80_D8      9       /* a byte:                           ADI 12H        */
#define K80_A16     10      /* an address:                       JMP 1234H      */
#define K80_PORT    11      /* a port:                           OUT 12H        */
#define K80_RST     12      /* a number 0 to 7 (bits 3-5):       RST 1          */

/* { mnemonic, opcode with the varying bits zero, what varies }. The ones that
 * stand alone come first, so that the MOV family does not claim HLT. */
#define ISA8080_OPS { \
    { "NOP",  0x00, K80_NONE }, { "RLC",  0x07, K80_NONE }, { "RRC",  0x0F, K80_NONE }, \
    { "RAL",  0x17, K80_NONE }, { "RAR",  0x1F, K80_NONE }, { "DAA",  0x27, K80_NONE }, \
    { "CMA",  0x2F, K80_NONE }, { "STC",  0x37, K80_NONE }, { "CMC",  0x3F, K80_NONE }, \
    { "HLT",  0x76, K80_NONE }, { "RET",  0xC9, K80_NONE }, { "XTHL", 0xE3, K80_NONE }, \
    { "PCHL", 0xE9, K80_NONE }, { "XCHG", 0xEB, K80_NONE }, { "DI",   0xF3, K80_NONE }, \
    { "SPHL", 0xF9, K80_NONE }, { "EI",   0xFB, K80_NONE }, \
    { "SHLD", 0x22, K80_A16 }, { "LHLD", 0x2A, K80_A16 }, { "STA",  0x32, K80_A16 }, \
    { "LDA",  0x3A, K80_A16 }, { "JMP",  0xC3, K80_A16 }, { "CALL", 0xCD, K80_A16 }, \
    { "JNZ",  0xC2, K80_A16 }, { "JZ",   0xCA, K80_A16 }, { "JNC",  0xD2, K80_A16 }, \
    { "JC",   0xDA, K80_A16 }, { "JPO",  0xE2, K80_A16 }, { "JPE",  0xEA, K80_A16 }, \
    { "JP",   0xF2, K80_A16 }, { "JM",   0xFA, K80_A16 }, \
    { "CNZ",  0xC4, K80_A16 }, { "CZ",   0xCC, K80_A16 }, { "CNC",  0xD4, K80_A16 }, \
    { "CC",   0xDC, K80_A16 }, { "CPO",  0xE4, K80_A16 }, { "CPE",  0xEC, K80_A16 }, \
    { "CP",   0xF4, K80_A16 }, { "CM",   0xFC, K80_A16 }, \
    { "RNZ",  0xC0, K80_NONE }, { "RZ",   0xC8, K80_NONE }, { "RNC",  0xD0, K80_NONE }, \
    { "RC",   0xD8, K80_NONE }, { "RPO",  0xE0, K80_NONE }, { "RPE",  0xE8, K80_NONE }, \
    { "RP",   0xF0, K80_NONE }, { "RM",   0xF8, K80_NONE }, \
    { "ADI",  0xC6, K80_D8 }, { "ACI",  0xCE, K80_D8 }, { "SUI",  0xD6, K80_D8 }, \
    { "SBI",  0xDE, K80_D8 }, { "ANI",  0xE6, K80_D8 }, { "XRI",  0xEE, K80_D8 }, \
    { "ORI",  0xF6, K80_D8 }, { "CPI",  0xFE, K80_D8 }, \
    { "OUT",  0xD3, K80_PORT }, { "IN",   0xDB, K80_PORT }, \
    { "RST",  0xC7, K80_RST }, \
    { "MOV",  0x40, K80_MOV }, { "MVI",  0x06, K80_MVI }, \
    { "INR",  0x04, K80_R3D }, { "DCR",  0x05, K80_R3D }, \
    { "ADD",  0x80, K80_R3 }, { "ADC",  0x88, K80_R3 }, { "SUB",  0x90, K80_R3 }, \
    { "SBB",  0x98, K80_R3 }, { "ANA",  0xA0, K80_R3 }, { "XRA",  0xA8, K80_R3 }, \
    { "ORA",  0xB0, K80_R3 }, { "CMP",  0xB8, K80_R3 }, \
    { "LXI",  0x01, K80_LXI }, { "INX",  0x03, K80_RP }, { "DCX",  0x0B, K80_RP }, \
    { "DAD",  0x09, K80_RP }, { "STAX", 0x02, K80_STAX }, { "LDAX", 0x0A, K80_STAX }, \
    { "PUSH", 0xC5, K80_RPP }, { "POP",  0xC1, K80_RPP } }

#define ISA8080_REGS    { "B", "C", "D", "E", "H", "L", "M", "A" }
#define ISA8080_PAIRS   { "B", "D", "H", "SP" }
#define ISA8080_PAIRS_P { "B", "D", "H", "PSW" }

#endif /* ISA_8080_CONST_H */
