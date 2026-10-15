/*
 * mcard.h - the 1802 Membership Card: a CDP1802 at 4 MHz with 32K of RAM at
 * 0000, the optional memory chip at 8000 (the ROM images), a serial line on Q
 * and EF3, and a front panel with eight data switches, eight data LEDs and a
 * hex display, an IN button, and the WAIT, CLR and READ/WRITE switches.
 *
 * The front panel works like the Netronics Elf's: CLR and WAIT make the four
 * modes (RUN both up, CLEAR with CLR down, WAIT with WAIT down, LOAD both
 * down); in LOAD the IN button writes the data switches to memory at R0 and
 * steps R0 on (or, with the READ/WRITE switch down so the memory is protected,
 * shows the byte there). In RUN the IN button pulls EF4 low. Rev L: if IN is
 * held while CLR goes up the program starts at 8000.
 *
 * Portable ISO C. Nothing here knows about the Elf II (elf.c).
 *
 * The board is Lee Hart's 1802 Membership Card (Rev L):
 * https://www.sunrise-ev.com/1802.htm and
 * https://www.retrotechnology.com/memship/memship.html
 *
 * Author: Thomas Dzubin
 */
#ifndef MCARD_H
#define MCARD_H

#include <stdbool.h>
#include <stdint.h>

#include "cpu1802.h"
#include "machine.h"
#include "mcard_const.h"
#include "membus.h"
#include "serial.h"
#include "target.h"

typedef struct {
    cpu1802_t cpu;
    membus_t  bus;
    uint8_t   ram[MCARD_RAM_SIZE];
    uint8_t   rom[MCARD_MONITOR_MAX]; /* the built-in monitor, at MCARD_ROM_BASE */
    uint32_t  rom_length;           /* its length (0 when there is none)         */

    bool      sw_wait_up;           /* WAIT switch up (not waiting)              */
    bool      sw_clr_up;            /* CLR switch up (not clearing)              */
    bool      sw_write;             /* READ/WRITE switch up: memory can be written */
    uint8_t   data_switches;        /* the eight data switches, bit 7 to bit 0   */
    uint8_t   display;              /* the data LEDs and the hex display (OUT 4) */
    bool      in_button;            /* IN is held down                           */
    bool      in_pending;           /* a press of IN not yet taken by DMA        */
    bool      was_clear;            /* the CLR switch was down on the last cycle */
    bool      jump_pending;         /* Rev L: start at 8000 after the init cycle */

    serial_t  serial;               /* Q and EF3                                 */
} mcard_t;

/* Power on: the RAM cleared, the monitor ROM in place (if it was given), the
 * switches as the panel starts (WAIT up, CLR down, READ/WRITE up: so the card is
 * being cleared), serial at 4800 baud with Q inverted (Rev J and later). */
void mcard_init(mcard_t *m, const uint8_t *monitor, uint32_t monitor_length);

/* Make the memory map again, with the ROM images that are fitted now (the file
 * menu calls it after a ROM is fitted or taken out). */
void mcard_map_memory(mcard_t *m);

/* The panel. */
void mcard_set_switches(mcard_t *m, bool wait_up, bool clr_up, bool write_enable);
void mcard_data_switch(mcard_t *m, unsigned bit);   /* flip data switch 0 to 7  */
void mcard_in_button(mcard_t *m, bool down);

/* A key typed on the terminal's keyboard goes down the serial line. */
bool mcard_key(mcard_t *m, uint8_t code);

/* One machine cycle; the CPU_CYCLE_* kind (CPU_CYCLE_NONE if no time passed). */
int mcard_cycle(mcard_t *m);

/* Up to cycles machine cycles; stops early if no time is passing. */
uint32_t mcard_run(mcard_t *m, uint32_t cycles);

/* The RAM as program memory for the ROM screen and the tools (no extra menu lines). */
void mcard_program_target(mcard_t *m, program_target_t *t);

extern const machine_t mcard_machine;

#endif /* MCARD_H */
