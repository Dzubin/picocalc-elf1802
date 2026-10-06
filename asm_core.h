/*
 * asm_core.h - the assembler, for any processor. It does everything that does not
 * depend on the instruction set: lines, labels, expressions, the directives
 * (ORG, DB, DW, DS, EQU, END), two passes so that a branch can go forward, the
 * listing of what each line made, and the errors. The processor's isa_t (isa.h)
 * turns one instruction line into bytes, using the functions below for its
 * operands. Portable ISO C.
 *
 * The source is plain text, one statement a line:
 *
 *     [label:] [instruction operands] [; comment]
 *
 * A label ends in a colon and is up to ASM_NAME_MAX letters, digits and
 * underscores. The processor says how its numbers are written (hex by default
 * with # for decimal for the 1802, $ for hex for the 6502 and 6800, an H suffix
 * for the 8080); in every spelling 'A' is a character, * is the address of the
 * line, an expression can add and subtract (TABLE+2), and a leading < or > takes
 * the low or high byte. Directives: ORG address, DB bytes and "text" (FCB), DW
 * 16-bit words (FDB), DS count (RMB), NAME EQU value, END.
 *
 * Author: Thomas Dzubin
 */
#ifndef ASM_CORE_H
#define ASM_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "asm_const.h"
#include "isa.h"

typedef struct {
    uint16_t line;                      /* source line, from 1                 */
    char     text[ASM_ERROR_TEXT_MAX + 1];
} asm_error_t;

typedef struct {
    char     name[ASM_NAME_MAX + 1];    /* in capitals                         */
    uint16_t value;
} asm_symbol_t;

/* What one source line made, for a listing. */
#define ASM_LINE_BAD        0x01        /* the line has an error               */
#define ASM_LINE_FORWARD    0x02        /* the line used a label that pass 1 had not
                                           seen yet                             */
typedef struct {
    uint16_t address;                   /* where the line's code starts        */
    uint16_t count;                     /* bytes it made (0 for none)          */
    uint8_t  bytes[ASM_LISTING_BYTES];  /* the first of them                   */
    uint8_t  flags;                     /* ASM_LINE_*                          */
} asm_line_t;

/* Everything the assembler knows. It is big (about 30 KB), so the caller keeps
 * one in static memory. */
typedef struct asm_s {
    /* the answer */
    uint8_t      image[ASM_IMAGE_SIZE]; /* the bytes: image[address - base]    */
    uint8_t      used[ASM_IMAGE_SIZE / 8];   /* one bit per byte of the image written */
    asm_line_t   lines[ASM_MAX_LINES];
    int          line_count;
    asm_symbol_t symbols[ASM_MAX_SYMBOLS];
    int          symbol_count;
    asm_error_t  errors[ASM_MAX_ERRORS];     /* the first few errors           */
    int          error_count;           /* all of them                         */
    uint32_t     bytes;                 /* bytes of code and data made         */
    int          low, high;             /* lowest and highest address written
                                           (real addresses; -1 when nothing was) */
    uint32_t     base;                  /* the address image[0] stands for      */
    uint32_t     size;                  /* the bytes of image the code may use  */

    /* the assembler's working state */
    const isa_t *isa;                   /* the processor being assembled for   */
    int          pass;
    int          current;               /* index of the line being read        */
    uint32_t     location;              /* the address being assembled         */
    bool         ended;                 /* END was seen                        */
    bool         unresolved;            /* a label was not known yet           */
} asm_t;

/* Assemble source (length bytes of text; lines end with a line feed) for the
 * processor isa into a->image. The code goes into the window of the address space
 * from base for size bytes (the machine's RAM: size at most ASM_IMAGE_SIZE, a
 * bigger one is cut); an address outside it is an error. Labels, a->low and
 * a->high and the listing are real addresses; only the image is indexed from
 * base. Returns the number of errors, 0 when it assembled. */
int asm_assemble(const isa_t *isa, asm_t *a, uint32_t base, uint32_t size,
                 const char *source, size_t length);

/* ------------------------------------------------------------------ */
/*  For the processors' encoders                                        */
/* ------------------------------------------------------------------ */
/* Note an error on the current line (only the first on a line is kept). */
void asm_error(asm_t *a, const char *text);

/* Read an expression at *pp, in the processor's number spelling, and move *pp
 * past it. Returns false (with the error noted) if there is none. While a label
 * is not yet known (pass 1) its value is 0 and asm_forward() is true. */
bool asm_expression(asm_t *a, const char **pp, uint32_t *value);

/* Put a byte at the address being assembled and move on. */
void asm_emit(asm_t *a, uint8_t byte);

uint32_t asm_location(const asm_t *a);      /* the address of the line               */
int      asm_pass(const asm_t *a);          /* 1 or 2                                */

/* True if the line used a label that pass 1 had not defined yet when it read it.
 * An encoder that picks a short or a long form of an instruction by the value of
 * its operand must use the long one then, in both passes, so that the size of
 * the line does not change. */
bool asm_forward(const asm_t *a);

/* A value that has to fit a byte (or be a small negative number). */
bool asm_byte_value(asm_t *a, uint32_t v, uint8_t *out);

/* The rest of the text must be empty (spaces apart): notes an error if not. */
bool asm_at_end(asm_t *a, const char *p);

/* Small helpers for reading operands. */
const char *asm_skip_space(const char *p);
char asm_upper(char c);
bool asm_is_digit(char c);
bool asm_is_name_start(char c);
bool asm_is_name_char(char c);
int  asm_hex_digit(char c);                 /* 0 to 15, or -1                        */
size_t asm_read_word(const char **pp, char *word, size_t size);   /* in capitals    */

#endif /* ASM_CORE_H */
