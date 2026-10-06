/*
 * mc10.h - the TRS-80 MC-10 as a machine: the MC6803 processor (cpu6803.c) on
 * the memory map of the MC-10 (membus.c), the MC6847 video chip (mc6847.c),
 * the keyboard matrix, the one-bit sound (qaudio.c) and the cassette lines.
 *
 * The ROM (8K at 0xE000, the one Tandy shipped) is not part of this project:
 * it is fitted from a .ROM file through rom.c like any ROM image. With no ROM
 * over the reset vector a small built-in program writes a message on the screen
 * saying so.
 *
 * mc10_run() runs whole instructions; the video chip and the sound are kept in
 * step by the cycles each one took. Portable ISO C.
 *
 * Author: Thomas Dzubin
 */
#ifndef MC10_H
#define MC10_H

#include <stdbool.h>
#include <stdint.h>

#include "cpu6803.h"
#include "machine.h"
#include "mc10_const.h"
#include "mc6847.h"
#include "membus.h"
#include "qaudio.h"

/* The memory map points into this structure (the RAM and the message page) and
 * the processor and the video chip carry its address as their ctx, so an mc10_t
 * must not be copied or moved once mc10_init() has run: set it up in place, in
 * the memory it will stay in. */
typedef struct mc10 {
    cpu6803_t cpu;
    membus_t  bus;
    mc6847_t  vdg;
    qaudio_t  audio;

    uint8_t   ram[MC10_RAM_MAX];
    uint32_t  ram_size;                 /* MC10_RAM_4K or MC10_RAM_20K        */
    uint8_t   fallback[MEMBUS_PAGE_SIZE];   /* the page at 0xFF00 with the message */

    uint8_t   keys[MC10_ROWS];          /* a bit set for each key that is down */
    uint8_t   plain[MC10_ROWS];         /* the keys held as themselves         */
    uint8_t   symbol[MC10_ROWS];        /* the keys held as a shifted symbol   */
    uint8_t   strobe;                   /* port 1: a low bit selects a row    */
    bool      dac;                      /* the sound output                   */
    bool      cassette_out;             /* port 2 bit 0                       */
    bool      cassette_in;              /* the level the cassette input has   */
} mc10_t;

/* Set up the machine with ram_size bytes of RAM (MC10_RAM_4K or MC10_RAM_20K)
 * and reset it. */
void mc10_init(mc10_t *m, uint32_t ram_size);

/* Map the memory again (call it after the ROM images change), fitting the ROM
 * images over the RAM and the built-in message program. */
void mc10_map_memory(mc10_t *m);

/* Reset the processor and the video chip; the RAM keeps what it had. */
void mc10_reset(mc10_t *m);

/* Run at least cycles processor cycles (a whole number of instructions); returns
 * how many ran. */
uint32_t mc10_run(mc10_t *m, uint32_t cycles);

/* Like mc10_run, but stop when the next instruction is at address (never before
 * one instruction has run); *hit says whether it stopped for that. */
uint32_t mc10_run_to(mc10_t *m, uint32_t cycles, uint16_t address, bool *hit);

/* A key of the matrix goes down or up. */
void mc10_key(mc10_t *m, int row, int bit, bool down);

/* The key that types a character (letters in either case, digits, the symbols,
 * MC10_KEY_ENTER, ' ' and the MC10_KEY_BREAK, MC10_KEY_CONTROL and MC10_KEY_SHIFT
 * codes) goes down or up; a symbol that is SHIFT of another key holds SHIFT too.
 * Returns false if there is no such key. */
bool mc10_press_char(mc10_t *m, int ch, bool down);

/* Let go of every key. */
void mc10_release_all(mc10_t *m);

/* The level of the cassette input line (what a tape recorder's earphone gives,
 * as a logic level). */
void mc10_set_cassette_in(mc10_t *m, bool level);

/* A byte of memory as the processor sees it, without side effects. */
uint8_t mc10_peek(const mc10_t *m, uint16_t addr);

extern const machine_t mc10_machine;

#endif /* MC10_H */
