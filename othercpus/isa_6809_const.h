/*
 * isa_6809_const.h - the instruction table of the Motorola 6809's description
 * (isa_6809.c): every documented opcode, with its mnemonic and addressing mode.
 * Opcodes with a prefix are written with the prefix in the high byte (1021 is
 * 10 21, LBRN). The undocumented opcodes are not in it; a listing writes them as
 * data. The table was made from the published instruction set and checked against
 * an independent one.
 *
 * Author: Thomas Dzubin
 */
#ifndef ISA_6809_CONST_H
#define ISA_6809_CONST_H

/* Addressing modes. */
#define M09_INH     0       /* inherent:              RTS                    */
#define M09_IMM8    1       /* immediate byte:        LDA #$12               */
#define M09_IMM16   2       /* immediate word:        LDD #$1234             */
#define M09_DIR     3       /* direct (the DP page):  LDA <$12               */
#define M09_IDX     4       /* indexed, with a postbyte: LDA 5,X  LDA ,X++   */
#define M09_EXT     5       /* extended:              LDA $1234              */
#define M09_REL     6       /* 8-bit relative branch: BNE $1234              */
#define M09_LREL    7       /* 16-bit relative branch: LBNE $1234            */
#define M09_RR      8       /* two registers:         TFR A,B                */
#define M09_PSHS    9       /* a register list, for the S stack: PSHS A,B,X  */
#define M09_PSHU    10      /*   ... and for the U stack                     */
#define M09_PULS    11
#define M09_PULU    12

/* { opcode (prefix in the high byte), mnemonic, mode } */
#define ISA6809_OPS {     { 0x0000, "NEG", M09_DIR }, { 0x0003, "COM", M09_DIR }, { 0x0004, "LSR", M09_DIR }, \
    { 0x0006, "ROR", M09_DIR }, { 0x0007, "ASR", M09_DIR }, { 0x0008, "ASL", M09_DIR }, \
    { 0x0009, "ROL", M09_DIR }, { 0x000A, "DEC", M09_DIR }, { 0x000C, "INC", M09_DIR }, \
    { 0x000D, "TST", M09_DIR }, { 0x000E, "JMP", M09_DIR }, { 0x000F, "CLR", M09_DIR }, \
    { 0x0012, "NOP", M09_INH }, { 0x0013, "SYNC", M09_INH }, { 0x0016, "LBRA", M09_LREL }, \
    { 0x0017, "LBSR", M09_LREL }, { 0x0019, "DAA", M09_INH }, { 0x001A, "ORCC", M09_IMM8 }, \
    { 0x001C, "ANDCC", M09_IMM8 }, { 0x001D, "SEX", M09_INH }, { 0x001E, "EXG", M09_RR }, \
    { 0x001F, "TFR", M09_RR }, { 0x0020, "BRA", M09_REL }, { 0x0021, "BRN", M09_REL }, \
    { 0x0022, "BHI", M09_REL }, { 0x0023, "BLS", M09_REL }, { 0x0024, "BCC", M09_REL }, \
    { 0x0025, "BCS", M09_REL }, { 0x0026, "BNE", M09_REL }, { 0x0027, "BEQ", M09_REL }, \
    { 0x0028, "BVC", M09_REL }, { 0x0029, "BVS", M09_REL }, { 0x002A, "BPL", M09_REL }, \
    { 0x002B, "BMI", M09_REL }, { 0x002C, "BGE", M09_REL }, { 0x002D, "BLT", M09_REL }, \
    { 0x002E, "BGT", M09_REL }, { 0x002F, "BLE", M09_REL }, { 0x0030, "LEAX", M09_IDX }, \
    { 0x0031, "LEAY", M09_IDX }, { 0x0032, "LEAS", M09_IDX }, { 0x0033, "LEAU", M09_IDX }, \
    { 0x0034, "PSHS", M09_PSHS }, { 0x0035, "PULS", M09_PULS }, { 0x0036, "PSHU", M09_PSHU }, \
    { 0x0037, "PULU", M09_PULU }, { 0x0039, "RTS", M09_INH }, { 0x003A, "ABX", M09_INH }, \
    { 0x003B, "RTI", M09_INH }, { 0x003C, "CWAI", M09_IMM8 }, { 0x003D, "MUL", M09_INH }, \
    { 0x003F, "SWI", M09_INH }, { 0x0040, "NEGA", M09_INH }, { 0x0043, "COMA", M09_INH }, \
    { 0x0044, "LSRA", M09_INH }, { 0x0046, "RORA", M09_INH }, { 0x0047, "ASRA", M09_INH }, \
    { 0x0048, "ASLA", M09_INH }, { 0x0049, "ROLA", M09_INH }, { 0x004A, "DECA", M09_INH }, \
    { 0x004C, "INCA", M09_INH }, { 0x004D, "TSTA", M09_INH }, { 0x004F, "CLRA", M09_INH }, \
    { 0x0050, "NEGB", M09_INH }, { 0x0053, "COMB", M09_INH }, { 0x0054, "LSRB", M09_INH }, \
    { 0x0056, "RORB", M09_INH }, { 0x0057, "ASRB", M09_INH }, { 0x0058, "ASLB", M09_INH }, \
    { 0x0059, "ROLB", M09_INH }, { 0x005A, "DECB", M09_INH }, { 0x005C, "INCB", M09_INH }, \
    { 0x005D, "TSTB", M09_INH }, { 0x005F, "CLRB", M09_INH }, { 0x0060, "NEG", M09_IDX }, \
    { 0x0063, "COM", M09_IDX }, { 0x0064, "LSR", M09_IDX }, { 0x0066, "ROR", M09_IDX }, \
    { 0x0067, "ASR", M09_IDX }, { 0x0068, "ASL", M09_IDX }, { 0x0069, "ROL", M09_IDX }, \
    { 0x006A, "DEC", M09_IDX }, { 0x006C, "INC", M09_IDX }, { 0x006D, "TST", M09_IDX }, \
    { 0x006E, "JMP", M09_IDX }, { 0x006F, "CLR", M09_IDX }, { 0x0070, "NEG", M09_EXT }, \
    { 0x0073, "COM", M09_EXT }, { 0x0074, "LSR", M09_EXT }, { 0x0076, "ROR", M09_EXT }, \
    { 0x0077, "ASR", M09_EXT }, { 0x0078, "ASL", M09_EXT }, { 0x0079, "ROL", M09_EXT }, \
    { 0x007A, "DEC", M09_EXT }, { 0x007C, "INC", M09_EXT }, { 0x007D, "TST", M09_EXT }, \
    { 0x007E, "JMP", M09_EXT }, { 0x007F, "CLR", M09_EXT }, { 0x0080, "SUBA", M09_IMM8 }, \
    { 0x0081, "CMPA", M09_IMM8 }, { 0x0082, "SBCA", M09_IMM8 }, { 0x0083, "SUBD", M09_IMM16 }, \
    { 0x0084, "ANDA", M09_IMM8 }, { 0x0085, "BITA", M09_IMM8 }, { 0x0086, "LDA", M09_IMM8 }, \
    { 0x0088, "EORA", M09_IMM8 }, { 0x0089, "ADCA", M09_IMM8 }, { 0x008A, "ORA", M09_IMM8 }, \
    { 0x008B, "ADDA", M09_IMM8 }, { 0x008C, "CMPX", M09_IMM16 }, { 0x008D, "BSR", M09_REL }, \
    { 0x008E, "LDX", M09_IMM16 }, { 0x0090, "SUBA", M09_DIR }, { 0x0091, "CMPA", M09_DIR }, \
    { 0x0092, "SBCA", M09_DIR }, { 0x0093, "SUBD", M09_DIR }, { 0x0094, "ANDA", M09_DIR }, \
    { 0x0095, "BITA", M09_DIR }, { 0x0096, "LDA", M09_DIR }, { 0x0097, "STA", M09_DIR }, \
    { 0x0098, "EORA", M09_DIR }, { 0x0099, "ADCA", M09_DIR }, { 0x009A, "ORA", M09_DIR }, \
    { 0x009B, "ADDA", M09_DIR }, { 0x009C, "CMPX", M09_DIR }, { 0x009D, "JSR", M09_DIR }, \
    { 0x009E, "LDX", M09_DIR }, { 0x009F, "STX", M09_DIR }, { 0x00A0, "SUBA", M09_IDX }, \
    { 0x00A1, "CMPA", M09_IDX }, { 0x00A2, "SBCA", M09_IDX }, { 0x00A3, "SUBD", M09_IDX }, \
    { 0x00A4, "ANDA", M09_IDX }, { 0x00A5, "BITA", M09_IDX }, { 0x00A6, "LDA", M09_IDX }, \
    { 0x00A7, "STA", M09_IDX }, { 0x00A8, "EORA", M09_IDX }, { 0x00A9, "ADCA", M09_IDX }, \
    { 0x00AA, "ORA", M09_IDX }, { 0x00AB, "ADDA", M09_IDX }, { 0x00AC, "CMPX", M09_IDX }, \
    { 0x00AD, "JSR", M09_IDX }, { 0x00AE, "LDX", M09_IDX }, { 0x00AF, "STX", M09_IDX }, \
    { 0x00B0, "SUBA", M09_EXT }, { 0x00B1, "CMPA", M09_EXT }, { 0x00B2, "SBCA", M09_EXT }, \
    { 0x00B3, "SUBD", M09_EXT }, { 0x00B4, "ANDA", M09_EXT }, { 0x00B5, "BITA", M09_EXT }, \
    { 0x00B6, "LDA", M09_EXT }, { 0x00B7, "STA", M09_EXT }, { 0x00B8, "EORA", M09_EXT }, \
    { 0x00B9, "ADCA", M09_EXT }, { 0x00BA, "ORA", M09_EXT }, { 0x00BB, "ADDA", M09_EXT }, \
    { 0x00BC, "CMPX", M09_EXT }, { 0x00BD, "JSR", M09_EXT }, { 0x00BE, "LDX", M09_EXT }, \
    { 0x00BF, "STX", M09_EXT }, { 0x00C0, "SUBB", M09_IMM8 }, { 0x00C1, "CMPB", M09_IMM8 }, \
    { 0x00C2, "SBCB", M09_IMM8 }, { 0x00C3, "ADDD", M09_IMM16 }, { 0x00C4, "ANDB", M09_IMM8 }, \
    { 0x00C5, "BITB", M09_IMM8 }, { 0x00C6, "LDB", M09_IMM8 }, { 0x00C8, "EORB", M09_IMM8 }, \
    { 0x00C9, "ADCB", M09_IMM8 }, { 0x00CA, "ORB", M09_IMM8 }, { 0x00CB, "ADDB", M09_IMM8 }, \
    { 0x00CC, "LDD", M09_IMM16 }, { 0x00CE, "LDU", M09_IMM16 }, { 0x00D0, "SUBB", M09_DIR }, \
    { 0x00D1, "CMPB", M09_DIR }, { 0x00D2, "SBCB", M09_DIR }, { 0x00D3, "ADDD", M09_DIR }, \
    { 0x00D4, "ANDB", M09_DIR }, { 0x00D5, "BITB", M09_DIR }, { 0x00D6, "LDB", M09_DIR }, \
    { 0x00D7, "STB", M09_DIR }, { 0x00D8, "EORB", M09_DIR }, { 0x00D9, "ADCB", M09_DIR }, \
    { 0x00DA, "ORB", M09_DIR }, { 0x00DB, "ADDB", M09_DIR }, { 0x00DC, "LDD", M09_DIR }, \
    { 0x00DD, "STD", M09_DIR }, { 0x00DE, "LDU", M09_DIR }, { 0x00DF, "STU", M09_DIR }, \
    { 0x00E0, "SUBB", M09_IDX }, { 0x00E1, "CMPB", M09_IDX }, { 0x00E2, "SBCB", M09_IDX }, \
    { 0x00E3, "ADDD", M09_IDX }, { 0x00E4, "ANDB", M09_IDX }, { 0x00E5, "BITB", M09_IDX }, \
    { 0x00E6, "LDB", M09_IDX }, { 0x00E7, "STB", M09_IDX }, { 0x00E8, "EORB", M09_IDX }, \
    { 0x00E9, "ADCB", M09_IDX }, { 0x00EA, "ORB", M09_IDX }, { 0x00EB, "ADDB", M09_IDX }, \
    { 0x00EC, "LDD", M09_IDX }, { 0x00ED, "STD", M09_IDX }, { 0x00EE, "LDU", M09_IDX }, \
    { 0x00EF, "STU", M09_IDX }, { 0x00F0, "SUBB", M09_EXT }, { 0x00F1, "CMPB", M09_EXT }, \
    { 0x00F2, "SBCB", M09_EXT }, { 0x00F3, "ADDD", M09_EXT }, { 0x00F4, "ANDB", M09_EXT }, \
    { 0x00F5, "BITB", M09_EXT }, { 0x00F6, "LDB", M09_EXT }, { 0x00F7, "STB", M09_EXT }, \
    { 0x00F8, "EORB", M09_EXT }, { 0x00F9, "ADCB", M09_EXT }, { 0x00FA, "ORB", M09_EXT }, \
    { 0x00FB, "ADDB", M09_EXT }, { 0x00FC, "LDD", M09_EXT }, { 0x00FD, "STD", M09_EXT }, \
    { 0x00FE, "LDU", M09_EXT }, { 0x00FF, "STU", M09_EXT }, { 0x1021, "LBRN", M09_LREL }, \
    { 0x1022, "LBHI", M09_LREL }, { 0x1023, "LBLS", M09_LREL }, { 0x1024, "LBCC", M09_LREL }, \
    { 0x1025, "LBCS", M09_LREL }, { 0x1026, "LBNE", M09_LREL }, { 0x1027, "LBEQ", M09_LREL }, \
    { 0x1028, "LBVC", M09_LREL }, { 0x1029, "LBVS", M09_LREL }, { 0x102A, "LBPL", M09_LREL }, \
    { 0x102B, "LBMI", M09_LREL }, { 0x102C, "LBGE", M09_LREL }, { 0x102D, "LBLT", M09_LREL }, \
    { 0x102E, "LBGT", M09_LREL }, { 0x102F, "LBLE", M09_LREL }, { 0x103F, "SWI2", M09_INH }, \
    { 0x1083, "CMPD", M09_IMM16 }, { 0x108C, "CMPY", M09_IMM16 }, { 0x108E, "LDY", M09_IMM16 }, \
    { 0x1093, "CMPD", M09_DIR }, { 0x109C, "CMPY", M09_DIR }, { 0x109E, "LDY", M09_DIR }, \
    { 0x109F, "STY", M09_DIR }, { 0x10A3, "CMPD", M09_IDX }, { 0x10AC, "CMPY", M09_IDX }, \
    { 0x10AE, "LDY", M09_IDX }, { 0x10AF, "STY", M09_IDX }, { 0x10B3, "CMPD", M09_EXT }, \
    { 0x10BC, "CMPY", M09_EXT }, { 0x10BE, "LDY", M09_EXT }, { 0x10BF, "STY", M09_EXT }, \
    { 0x10CE, "LDS", M09_IMM16 }, { 0x10DE, "LDS", M09_DIR }, { 0x10DF, "STS", M09_DIR }, \
    { 0x10EE, "LDS", M09_IDX }, { 0x10EF, "STS", M09_IDX }, { 0x10FE, "LDS", M09_EXT }, \
    { 0x10FF, "STS", M09_EXT }, { 0x113F, "SWI3", M09_INH }, { 0x1183, "CMPU", M09_IMM16 }, \
    { 0x118C, "CMPS", M09_IMM16 }, { 0x1193, "CMPU", M09_DIR }, { 0x119C, "CMPS", M09_DIR }, \
    { 0x11A3, "CMPU", M09_IDX }, { 0x11AC, "CMPS", M09_IDX }, { 0x11B3, "CMPU", M09_EXT }, \
    { 0x11BC, "CMPS", M09_EXT } }

/* The registers of TFR and EXG, by their code (a 4-bit field; NULL where there is
 * none), and the register lists of PSHS and its relatives by bit. */
#define ISA6809_RR_NAMES { "D", "X", "Y", "U", "S", "PC", NULL, NULL,                            "A", "B", "CC", "DP", NULL, NULL, NULL, NULL }
#define ISA6809_LIST_NAMES { "CC", "A", "B", "DP", "X", "Y", "U", "PC" }   /* bit 6 is S for the U stack */

#endif /* ISA_6809_CONST_H */
