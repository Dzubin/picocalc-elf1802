/*
 * target.h - the program memory of a machine, as the editor and the file menu see
 * it: the bytes of its address space, which processor it is (for the assembler and
 * the listing), and what to do when a new program has been put in it. They work on
 * this and know nothing of any one machine; a machine fills one in
 * (elf_program_target() for the Elf). Portable ISO C.
 *
 * Author: Thomas Dzubin
 */
#ifndef TARGET_H
#define TARGET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "isa.h"
#include "membus.h"

typedef struct {
    uint8_t     *ram;               /* the bytes of the program's memory (RAM)         */
    uint32_t     base;              /* the address of ram[0] in the machine's memory   */
    uint32_t     size;              /* how many bytes                                  */
    const isa_t *isa;               /* the processor, for the assembler and the listing */

    /* Called after the RAM has been replaced by a new program (clear what a new
     * program needs cleared: the VDU of the Elf). May be NULL. */
    void       (*loaded)(void *ctx);

    /* A line of the file menu for something of the machine's own (the Elf's 1861):
     * the key, the text of the line, and what pressing the key does (it returns
     * true if the screen has to be redrawn, and may put a short note in note).
     * extra_key is 0 if there is none. */
    char         extra_key;
    const char *(*extra_text)(void *ctx);
    bool       (*extra_press)(void *ctx, char *note, size_t note_size);

    membus_t    *bus;               /* the memory map, for the ROM images (NULL if none) */
    void       (*remap)(void *ctx); /* map the memory again, with the ROM images that are
                                       fitted now (NULL if the machine has no memory map) */

    /* A line of the file menu to choose how much RAM the machine has: the key,
     * the text of the line, and what pressing it does (the next size; it puts a
     * note in note and changes size above). ram_key is 0 if the size is fixed. */
    char         ram_key;
    const char *(*ram_text)(void *ctx);
    void       (*ram_press)(void *ctx, char *note, size_t note_size);

    void        *ctx;               /* what the functions above are given     */
} program_target_t;

/* Put a program in the RAM: len bytes of data (at most size), the rest zeros,
 * and tell the machine. data may be NULL with len 0 to clear it. */
void program_target_load(const program_target_t *t, const uint8_t *data, uint32_t len);

#endif /* TARGET_H */
