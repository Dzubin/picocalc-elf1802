/*
 * elf.h - the whole computer: a CDP1802 and a CDP1861 on a board with 16K of
 * RAM, a hexadecimal keypad with an IN button, two hex displays, a Q LED and
 * the RUN, LOAD and memory-protect switches, in the style of the Netronics
 * Elf II.
 *
 * What the front panel does:
 *
 *   RUN down, LOAD down   RESET: the CPU is held, R0 will be cleared
 *   RUN down, LOAD up     LOAD:  the CPU idles; type two hex digits on the
 *                         keypad and press IN to store them at R0, which then
 *                         steps on (the display shows the byte)
 *   RUN up,   LOAD down   RUN:   the program runs from address 0000
 *   RUN up,   LOAD up     PAUSE: the CPU is stopped where it is
 *   MP up                 memory protect: nothing can write the RAM
 *
 * The keypad keeps the last two digits typed (the Elf's two-key latch); INP 4
 * reads them. EF4 is low while IN is held down. OUT 4 sets the displays.
 * INP 1 turns the video on and OUT 1 turns it off.
 *
 * Two more boards are plugged in. An ASCII keyboard: INP 7 returns the code of
 * the last key pressed (it stays in the latch) and EF3 is low until the code
 * has been read, after which the next waiting key comes up a little later.
 * A VDU, a screen kept in 1K of video RAM at E000 to E3FF (see VDU_* in
 * elf_const.h): the program reads and writes it like memory, and the front end
 * draws it. Memory protect does not cover the VDU's RAM.
 *
 * Portable ISO C. Constants are in elf_const.h.
 *
 * Author: Thomas Dzubin
 */
#ifndef ELF_H
#define ELF_H

#include <stdbool.h>
#include <stdint.h>

#include "cdp1861.h"
#include "cpu1802.h"
#include "elf_const.h"
#include "machine.h"
#include "membus.h"
#include "qaudio.h"
#include "target.h"

typedef struct {
    cpu1802_t cpu;
    pixie_t   video;
    bool      video_installed;      /* the 1861 is on the board (the Elf II);
                                       off is a basic Elf: an 1802, RAM, keypad,
                                       displays and Q, with EF1, INT and DMA
                                       free and ports 1 doing nothing          */
    membus_t  bus;                  /* the memory map: the RAM below, the VDU's
                                       video RAM, and nothing else            */
    uint8_t   ram[ELF_RAM_SIZE];

    bool      sw_run;               /* RUN switch up                         */
    bool      sw_load;              /* LOAD switch up                        */
    bool      sw_mp;                /* memory protect on                     */

    uint8_t   key_latch;            /* the last two hex digits typed         */
    uint8_t   display;              /* what the two hex displays show        */
    bool      in_button;            /* IN is held down                       */
    bool      in_pending;           /* a press of IN not yet taken by DMA    */

    uint32_t  q_changes;            /* times Q has changed (for a beeper)    */
    uint32_t  q_period;             /* machine cycles between the last two
                                       times Q went high, 0 if not known     */
    uint32_t  q_rise_cycle;         /* the cycle count when it last went high */
    bool      q_rise_known;
    qaudio_t  audio;                /* Q as sound, a sample every few cycles  */

    uint8_t   vdu[ELF_VDU_SIZE];    /* the VDU's video RAM at ELF_VDU_BASE   */
    uint32_t  vdu_writes;           /* writes to it, to tell when it is used */

    uint32_t  kbd_polls;            /* times a program has fetched B3 or BN3
                                       (testing EF3) or run INP 7: it wants
                                       the ASCII keyboard                    */
    uint8_t   kbd[ELF_KBD_FIFO];    /* ASCII keys waiting their turn         */
    uint8_t   kbd_head;
    uint8_t   kbd_count;
    uint8_t   kbd_latch;            /* the code INP 7 gives                  */
    bool      kbd_ready;            /* a new key is in the latch (EF3 low)  */
    uint32_t  kbd_gap;              /* cycles before the next key may show   */
} elf_t;

void elf_init(elf_t *m);            /* power on: RAM cleared, in RESET       */

/* Fit the 1861 video chip to the board, or take it off (a basic Elf). Without
 * it EF1 stays high, there is no interrupt or DMA from it, and INP 1 and OUT 1
 * do nothing. It is fitted when the Elf is powered on. */
void elf_set_video(elf_t *m, bool installed);

/* The front panel. */
void elf_set_switches(elf_t *m, bool run, bool load, bool mp);
void elf_key_hex(elf_t *m, unsigned digit);     /* a hex key is pressed     */
void elf_in_button(elf_t *m, bool down);

/* A key arrives on the ASCII keyboard (port 7, flag EF3). Returns false and
 * drops it if the keys already waiting for their turn fill the buffer. */
bool elf_ascii_key(elf_t *m, uint8_t code);

/* Run one machine cycle; returns the CPU_CYCLE_* kind (CPU_CYCLE_NONE if no
 * time passed, as in RESET, PAUSE and an idle LOAD). */
int elf_cycle(elf_t *m);

/* Run up to "budget" machine cycles; stops early if no time is passing.
 * Returns the number of cycles run. */
uint32_t elf_run(elf_t *m, uint32_t budget);

/* Run one whole instruction (the cycles of any DMA or interrupt that come
 * first, then its fetch and execute cycles), for single-stepping. Stops at once
 * if no time is passing (RESET, PAUSE, idle in LOAD) and after ELF_STEP_LIMIT
 * cycles (an IDL that no interrupt ends). Returns the cycles run. */
uint32_t elf_step(elf_t *m);

/* Like elf_run(), but stops when the next instruction to be fetched is at
 * break_addr (never before one instruction has run, so it can be started from
 * a breakpoint). *hit says whether it stopped for that reason. */
uint32_t elf_run_to(elf_t *m, uint32_t budget, uint16_t break_addr, bool *hit);

/* Fill in the program memory of the Elf for the editor and the file menu: its RAM,
 * the 1802, clearing the VDU when a program is loaded, and the menu line for
 * fitting or removing the 1861 (the V key). */
void elf_program_target(elf_t *m, program_target_t *t);

/* The Elf as a machine for the shell: its clock and how to run it (self is an
 * elf_t). */
extern const machine_t elf_machine;

/* Read a byte as the CPU would, but without any side effect (for displays of
 * memory and the disassembler). */
uint8_t elf_peek(const elf_t *m, uint16_t addr);

/* Blank the VDU's video RAM (done when a program is loaded). */
void elf_clear_vdu(elf_t *m);

/* Read or write RAM from outside (loading a program, a display of memory). */
void elf_load_image(elf_t *m, const uint8_t *data, uint32_t len);

#endif /* ELF_H */
