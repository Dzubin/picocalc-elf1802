/*
 * isa.h - what the tools (the assembler, the listing of a program, the
 * debugger) need to know about a processor's instruction set. Each processor
 * supplies one isa_t: how to write an instruction as text, which way the program
 * goes after it, how its source text is spelled, and how to turn one line of it
 * into bytes. The tools work from that and know nothing of any one processor.
 *
 * Portable ISO C.
 *
 * Author: Thomas Dzubin
 */
#ifndef ISA_H
#define ISA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "isa_const.h"

struct asm_s;                       /* the assembler's state, asm_core.h */

/* What one instruction does to the flow of the program. */
typedef struct {
    int length;                     /* bytes it takes, 1 to ISA_MAX_BYTES      */
    int kind;                       /* ISA_FLOW_*                              */
    int target;                     /* the address a branch, jump or call goes to,
                                       or -1                                   */
    int skip;                       /* bytes it jumps over when it skips, or 0 */
} isa_flow_t;

typedef struct isa_s {
    const char *name;               /* "1802"                                  */
    int         max_bytes;          /* the longest instruction                 */
    bool        big_endian_words;   /* DW puts the high byte first             */
    unsigned    numbers;            /* ISA_NUM_*: how numbers are written      */

    /* Write the instruction at addr, whose first bytes are b[0] to
     * b[max_bytes - 1] (bytes it does not use are ignored), as text in the form
     * the assembler reads. If target is not NULL it is written in place of the
     * address of a branch, jump or call. Returns the length in bytes. */
    int  (*disasm)(uint16_t addr, const uint8_t *b, const char *target,
                   char *text, size_t size);

    /* The flow of the instruction at addr. */
    void (*flow)(uint16_t addr, const uint8_t *b, isa_flow_t *f);

    /* Optional: addresses the program goes to that no branch names (a register
     * loaded with an address that a jump through it uses). Looks at the code found
     * so far (starts has a bit for each address that starts an instruction, limit is
     * the end of the code) and stores up to max addresses in out; returns how many. */
    int  (*indirect_entries)(const uint8_t *image, uint32_t size, uint32_t limit,
                             const uint8_t *starts, uint16_t *out, int max);

    /* Make the bytes of one instruction of the assembler's source: the mnemonic
     * (in capitals, any ".X" suffix included) and the text of its operands.
     * Returns false if the mnemonic is not one of this processor's; if it is,
     * the function reports any error in the operands itself. */
    bool (*encode)(struct asm_s *a, const char *mnemonic, const char *operands);

    /* Optional: a name that cannot be a label (a register). */
    bool (*reserved)(const char *name);

    /* Optional: a short note for the listing after the instruction at b (what
     * the machine part it talks to is), or NULL. */
    const char *(*note)(const uint8_t *b);

    /* Optional, for a debugger: the register read-out, as rows of text. regs is
     * the processor's state, row counts from 0; returns false past the last. */
    bool (*register_row)(const void *regs, int row, char *text, size_t size);
} isa_t;

/* The description of the 1802, the Elf's processor. The ones for other processors
 * are in othercpus/ (isa_others.h). */
extern const isa_t isa_1802;

#endif /* ISA_H */
