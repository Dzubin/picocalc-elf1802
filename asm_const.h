/*
 * asm_const.h - constants of the assembler (asm_core.c) and of the listing made from
 * an image (source_gen.c): their limits, the directives and the text of the error
 * messages.
 *
 * Author: Thomas Dzubin
 */
#ifndef ASM_CONST_H
#define ASM_CONST_H

/* Limits. The assembler works on a source of up to ASM_MAX_LINES lines and
 * builds its answer in an image of up to ASM_IMAGE_SIZE bytes: the window of the
 * machine's address space (its RAM) that the caller names. The default is the
 * Elf's 16 KB of RAM; a program for a machine with more builds with
 * -DASM_IMAGE_SIZE=... (the tests in othercpus/ use 0x5000). It is the one static
 * buffer to raise for such a machine. */
#ifndef ASM_IMAGE_SIZE
#define ASM_IMAGE_SIZE      0x4000
#endif
#define ASM_MAX_LINES       1536
#define ASM_LINE_MAX        120     /* characters in a source line            */
#define ASM_MAX_SYMBOLS     512
#define ASM_NAME_MAX        12      /* characters in a label                  */
#define ASM_MAX_ERRORS      16      /* errors kept (all are counted)          */
#define ASM_ERROR_TEXT_MAX  31
#define ASM_MNEMONIC_MAX    8       /* characters in a mnemonic, a suffix included */
#define ASM_LISTING_BYTES   3       /* bytes of a line kept for the listing   */

/* The assembler directives. */
#define ASM_DIR_ORG         "ORG"   /* ORG address: continue at that address  */
#define ASM_DIR_DB          "DB"    /* DB a, b, "text": bytes                 */
#define ASM_DIR_DW          "DW"    /* DW a, b: 16-bit words, in the byte order of the processor */
#define ASM_DIR_DS          "DS"    /* DS n: leave n bytes (zeros)            */
#define ASM_DIR_EQU         "EQU"   /* NAME EQU value                         */
#define ASM_DIR_END         "END"   /* stop here                              */
#define ASM_DIR_FCB         "FCB"   /* the Motorola names for DB, DW and DS   */
#define ASM_DIR_FDB         "FDB"
#define ASM_DIR_RMB         "RMB"

/* The label that the source generator puts on the target of a branch, and the
 * line it starts the source with. */
#define ASM_LABEL_PREFIX    "L"

/* Error messages (at most ASM_ERROR_TEXT_MAX characters). */
#define ASM_ERR_MNEMONIC    "UNKNOWN INSTRUCTION"
#define ASM_ERR_OPERAND     "BAD OPERAND"
#define ASM_ERR_MISSING     "MISSING OPERAND"
#define ASM_ERR_EXTRA       "EXTRA TEXT AFTER THE OPERAND"
#define ASM_ERR_REGISTER    "BAD REGISTER (0 TO F)"
#define ASM_ERR_PORT        "BAD PORT (1 TO 7)"
#define ASM_ERR_LDN0        "LDN 0 IS IDL: USE IDL"
#define ASM_ERR_NUMBER      "BAD NUMBER"
#define ASM_ERR_BIG         "VALUE TOO BIG"
#define ASM_ERR_UNDEFINED   "UNDEFINED LABEL"
#define ASM_ERR_DUPLICATE   "LABEL DEFINED TWICE"
#define ASM_ERR_LABEL       "BAD LABEL NAME"
#define ASM_ERR_LABEL_NUM   "LABEL LOOKS LIKE A NUMBER"
#define ASM_ERR_SYMBOLS     "TOO MANY LABELS"
#define ASM_ERR_PAGE        "SHORT BRANCH OFF ITS PAGE"
#define ASM_ERR_RAM         "ADDRESS OUTSIDE THE RAM"
#define ASM_ERR_OVERLAP     "OVERLAPS EARLIER CODE"
#define ASM_ERR_KNOWN       "NEEDS A VALUE KNOWN BY HERE"
#define ASM_ERR_STRING      "UNFINISHED STRING"
#define ASM_ERR_LONG        "LINE TOO LONG"
#define ASM_ERR_LINES       "TOO MANY LINES"
#define ASM_ERR_EMPTY       "NOTHING TO ASSEMBLE"
#define ASM_ERR_MODE        "BAD OPERAND FOR THIS INSTRUCTION"
#define ASM_ERR_RANGE       "BRANCH TOO FAR"

/* A note in a listing (a comment after an instruction): the instruction text takes
 * ASM_NOTE_COLUMN columns from column 8, then " ; " and the note, which is at most
 * ASM_NOTE_MAX characters, so a line fits the 40 columns of the screen. */
#define ASM_NOTE_COLUMN     12
#define ASM_NOTE_MAX        17

/* Making source from an image (srcgen_from_image): how the trace for code
 * and the lines of data are limited. */
#define ASM_FLOW_STACK      1024    /* addresses waiting to be followed          */
#define ASM_FLOW_ENTRIES    64      /* addresses a processor says code is at, a round */
#define ASM_CAND_MAX        4       /* addresses remembered for each register    */
#define ASM_DB_BYTES        8       /* bytes of data on a DB line                */
#define ASM_DS_MIN          8       /* a run of zeros this long is a DS line     */
#define ASM_DS_MAX          0x3FFF  /* ... and at most this long                 */
#define ASM_TEXT_MIN        5       /* printable characters that make a string   */
#define ASM_TEXT_MAX        24      /* ... and the most on a line                */

#endif /* ASM_CONST_H */
