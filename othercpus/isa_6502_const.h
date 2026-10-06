/*
 * isa_6502_const.h - the instruction table of the MOS 6502's description
 * (isa_6502.c): every documented opcode, with its mnemonic and addressing mode.
 * The undocumented opcodes are not in it; a listing writes them as data.
 *
 * Author: Thomas Dzubin
 */
#ifndef ISA_6502_CONST_H
#define ISA_6502_CONST_H

/* Addressing modes. */
#define M65_IMP     0       /* implied:               RTS               */
#define M65_ACC     1       /* the accumulator:       ASL A             */
#define M65_IMM     2       /* immediate:             LDA #$12          */
#define M65_ZP      3       /* zero page:             LDA $12           */
#define M65_ZPX     4       /*                        LDA $12,X         */
#define M65_ZPY     5       /*                        LDX $12,Y         */
#define M65_ABS     6       /* absolute:              LDA $1234         */
#define M65_ABX     7       /*                        LDA $1234,X       */
#define M65_ABY     8       /*                        LDA $1234,Y       */
#define M65_IND     9       /* indirect:              JMP ($1234)       */
#define M65_IZX    10       /* indexed indirect:      LDA ($12,X)       */
#define M65_IZY    11       /* indirect indexed:      LDA ($12),Y       */
#define M65_REL    12       /* relative branch:       BNE $1234         */

/* { opcode, mnemonic, mode } */
#define ISA6502_OPCODES { \
    { 0x00, "BRK", M65_IMP }, { 0x01, "ORA", M65_IZX }, { 0x05, "ORA", M65_ZP  }, \
    { 0x06, "ASL", M65_ZP  }, { 0x08, "PHP", M65_IMP }, { 0x09, "ORA", M65_IMM }, \
    { 0x0A, "ASL", M65_ACC }, { 0x0D, "ORA", M65_ABS }, { 0x0E, "ASL", M65_ABS }, \
    { 0x10, "BPL", M65_REL }, { 0x11, "ORA", M65_IZY }, { 0x15, "ORA", M65_ZPX }, \
    { 0x16, "ASL", M65_ZPX }, { 0x18, "CLC", M65_IMP }, { 0x19, "ORA", M65_ABY }, \
    { 0x1D, "ORA", M65_ABX }, { 0x1E, "ASL", M65_ABX }, \
    { 0x20, "JSR", M65_ABS }, { 0x21, "AND", M65_IZX }, { 0x24, "BIT", M65_ZP  }, \
    { 0x25, "AND", M65_ZP  }, { 0x26, "ROL", M65_ZP  }, { 0x28, "PLP", M65_IMP }, \
    { 0x29, "AND", M65_IMM }, { 0x2A, "ROL", M65_ACC }, { 0x2C, "BIT", M65_ABS }, \
    { 0x2D, "AND", M65_ABS }, { 0x2E, "ROL", M65_ABS }, \
    { 0x30, "BMI", M65_REL }, { 0x31, "AND", M65_IZY }, { 0x35, "AND", M65_ZPX }, \
    { 0x36, "ROL", M65_ZPX }, { 0x38, "SEC", M65_IMP }, { 0x39, "AND", M65_ABY }, \
    { 0x3D, "AND", M65_ABX }, { 0x3E, "ROL", M65_ABX }, \
    { 0x40, "RTI", M65_IMP }, { 0x41, "EOR", M65_IZX }, { 0x45, "EOR", M65_ZP  }, \
    { 0x46, "LSR", M65_ZP  }, { 0x48, "PHA", M65_IMP }, { 0x49, "EOR", M65_IMM }, \
    { 0x4A, "LSR", M65_ACC }, { 0x4C, "JMP", M65_ABS }, { 0x4D, "EOR", M65_ABS }, \
    { 0x4E, "LSR", M65_ABS }, \
    { 0x50, "BVC", M65_REL }, { 0x51, "EOR", M65_IZY }, { 0x55, "EOR", M65_ZPX }, \
    { 0x56, "LSR", M65_ZPX }, { 0x58, "CLI", M65_IMP }, { 0x59, "EOR", M65_ABY }, \
    { 0x5D, "EOR", M65_ABX }, { 0x5E, "LSR", M65_ABX }, \
    { 0x60, "RTS", M65_IMP }, { 0x61, "ADC", M65_IZX }, { 0x65, "ADC", M65_ZP  }, \
    { 0x66, "ROR", M65_ZP  }, { 0x68, "PLA", M65_IMP }, { 0x69, "ADC", M65_IMM }, \
    { 0x6A, "ROR", M65_ACC }, { 0x6C, "JMP", M65_IND }, { 0x6D, "ADC", M65_ABS }, \
    { 0x6E, "ROR", M65_ABS }, \
    { 0x70, "BVS", M65_REL }, { 0x71, "ADC", M65_IZY }, { 0x75, "ADC", M65_ZPX }, \
    { 0x76, "ROR", M65_ZPX }, { 0x78, "SEI", M65_IMP }, { 0x79, "ADC", M65_ABY }, \
    { 0x7D, "ADC", M65_ABX }, { 0x7E, "ROR", M65_ABX }, \
    { 0x81, "STA", M65_IZX }, { 0x84, "STY", M65_ZP  }, { 0x85, "STA", M65_ZP  }, \
    { 0x86, "STX", M65_ZP  }, { 0x88, "DEY", M65_IMP }, { 0x8A, "TXA", M65_IMP }, \
    { 0x8C, "STY", M65_ABS }, { 0x8D, "STA", M65_ABS }, { 0x8E, "STX", M65_ABS }, \
    { 0x90, "BCC", M65_REL }, { 0x91, "STA", M65_IZY }, { 0x94, "STY", M65_ZPX }, \
    { 0x95, "STA", M65_ZPX }, { 0x96, "STX", M65_ZPY }, { 0x98, "TYA", M65_IMP }, \
    { 0x99, "STA", M65_ABY }, { 0x9A, "TXS", M65_IMP }, { 0x9D, "STA", M65_ABX }, \
    { 0xA0, "LDY", M65_IMM }, { 0xA1, "LDA", M65_IZX }, { 0xA2, "LDX", M65_IMM }, \
    { 0xA4, "LDY", M65_ZP  }, { 0xA5, "LDA", M65_ZP  }, { 0xA6, "LDX", M65_ZP  }, \
    { 0xA8, "TAY", M65_IMP }, { 0xA9, "LDA", M65_IMM }, { 0xAA, "TAX", M65_IMP }, \
    { 0xAC, "LDY", M65_ABS }, { 0xAD, "LDA", M65_ABS }, { 0xAE, "LDX", M65_ABS }, \
    { 0xB0, "BCS", M65_REL }, { 0xB1, "LDA", M65_IZY }, { 0xB4, "LDY", M65_ZPX }, \
    { 0xB5, "LDA", M65_ZPX }, { 0xB6, "LDX", M65_ZPY }, { 0xB8, "CLV", M65_IMP }, \
    { 0xB9, "LDA", M65_ABY }, { 0xBA, "TSX", M65_IMP }, { 0xBC, "LDY", M65_ABX }, \
    { 0xBD, "LDA", M65_ABX }, { 0xBE, "LDX", M65_ABY }, \
    { 0xC0, "CPY", M65_IMM }, { 0xC1, "CMP", M65_IZX }, { 0xC4, "CPY", M65_ZP  }, \
    { 0xC5, "CMP", M65_ZP  }, { 0xC6, "DEC", M65_ZP  }, { 0xC8, "INY", M65_IMP }, \
    { 0xC9, "CMP", M65_IMM }, { 0xCA, "DEX", M65_IMP }, { 0xCC, "CPY", M65_ABS }, \
    { 0xCD, "CMP", M65_ABS }, { 0xCE, "DEC", M65_ABS }, \
    { 0xD0, "BNE", M65_REL }, { 0xD1, "CMP", M65_IZY }, { 0xD5, "CMP", M65_ZPX }, \
    { 0xD6, "DEC", M65_ZPX }, { 0xD8, "CLD", M65_IMP }, { 0xD9, "CMP", M65_ABY }, \
    { 0xDD, "CMP", M65_ABX }, { 0xDE, "DEC", M65_ABX }, \
    { 0xE0, "CPX", M65_IMM }, { 0xE1, "SBC", M65_IZX }, { 0xE4, "CPX", M65_ZP  }, \
    { 0xE5, "SBC", M65_ZP  }, { 0xE6, "INC", M65_ZP  }, { 0xE8, "INX", M65_IMP }, \
    { 0xE9, "SBC", M65_IMM }, { 0xEA, "NOP", M65_IMP }, { 0xEC, "CPX", M65_ABS }, \
    { 0xED, "SBC", M65_ABS }, { 0xEE, "INC", M65_ABS }, \
    { 0xF0, "BEQ", M65_REL }, { 0xF1, "SBC", M65_IZY }, { 0xF5, "SBC", M65_ZPX }, \
    { 0xF6, "INC", M65_ZPX }, { 0xF8, "SED", M65_IMP }, { 0xF9, "SBC", M65_ABY }, \
    { 0xFD, "SBC", M65_ABX }, { 0xFE, "INC", M65_ABX } }

#endif /* ISA_6502_CONST_H */
