/*
 * isa_1802_const.h - constants of the CDP1802's instruction-set description
 * (isa_1802.c): other spellings of the instructions and the notes that a listing
 * puts after the ones that talk to the Elf's own parts.
 *
 * Author: Thomas Dzubin
 */
#ifndef ISA_1802_CONST_H
#define ISA_1802_CONST_H

#define ISA1802_MAX_MNEMONICS   128
#define ISA1802_MNEMONIC_MAX    5

/* Other names for the instructions, as some 1802 books spell them. */
#define ISA1802_ALIASES { \
    { "NBR",  "SKP"  }, \
    { "NLBR", "LSKP" }, \
    { "RSHR", "SHRC" }, \
    { "RSHL", "SHLC" } }

/* How many addresses a register is remembered to have been loaded with (for the
 * code that a SEP goes to). */
#define ISA1802_CAND_MAX        4

/* Notes for a listing after the instructions that talk to the Elf's own parts
 * (opcode, then the note; at most ASM_NOTE_MAX characters). The EF flags are true
 * when their pin is low (B1 to B4 branch on a low pin). */
#define ISA1802_NOTES { \
    { 0x64, "hex displays" },       /* OUT 4                      */ \
    { 0x6C, "hex keypad" },         /* INP 4                      */ \
    { 0x69, "1861 video on" },      /* INP 1                      */ \
    { 0x61, "1861 video off" },     /* OUT 1                      */ \
    { 0x6F, "ASCII keyboard" },     /* INP 7                      */ \
    { 0x7B, "Q on" },               /* SEQ                        */ \
    { 0x7A, "Q off" },              /* REQ                        */ \
    { 0x37, "if IN pressed" },      /* B4                         */ \
    { 0x3F, "if IN not pressed" },  /* BN4                        */ \
    { 0x36, "if key waiting" },     /* B3: the ASCII keyboard     */ \
    { 0x3E, "if no key waiting" },  /* BN3                        */ \
    { 0x34, "if 1861 EFX low" },    /* B1                         */ \
    { 0x3C, "if 1861 EFX high" } }  /* BN1                        */

/* The debugger's read-out of the registers. */
#define ISA1802_REG_ROWS        6   /* the flags, then four rows of four registers */

#endif /* ISA_1802_CONST_H */
