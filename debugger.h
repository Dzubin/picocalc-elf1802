/*
 * debugger.h - the debugger view (ESC menu, G): the registers, a disassembly listing
 * from the PC (or wherever it was scrolled to) with the targets of branches
 * marked, a dump of memory at the data pointer, single-stepping by whole
 * instructions and one breakpoint. The Elf is halted while the debugger is
 * open, except after G.
 *
 * The debugger draws on the whole screen and owns its own state, but it does
 * not run the Elf's main loop or redraw the Elf's screens: the caller asks it
 * to draw while it is active (debugger_draw()), runs the machine only while it
 * is not halted, stops it at a breakpoint (debugger_break_address() and
 * debugger_break_hit()), and redraws its own screen when a call below says the
 * debugger was left or entered. The debugger works on one machine, given to
 * debugger_attach().
 *
 * Author: Thomas Dzubin
 */
#ifndef DEBUGGER_H
#define DEBUGGER_H

#include <stdbool.h>
#include <stdint.h>

#include "debugger_const.h"
#include "elf.h"

void debugger_attach(elf_t *m);         /* once, before anything else         */
void debugger_reset(void);              /* back to the start-up state: closed,
                                           running, no breakpoint              */

/* Opening and closing it. Both leave the screen to the caller to redraw. */
void debugger_enter(void);              /* the Elf stops at the end of the
                                           instruction it is in                */
void debugger_leave(void);              /* the Elf runs again                  */

bool debugger_active(void);             /* the debugger is on the screen       */
bool debugger_halted(void);             /* the Elf is stopped for it           */

/* The breakpoint, or -1 for none; the main loop runs the Elf to it and calls
 * debugger_break_hit() when it stops there. */
int32_t debugger_break_address(void);
void    debugger_break_hit(void);

/* A key press while the debugger is active: DEBUGGER_KEY_PASS (not one of its
 * keys; the keypad and switches take it as usual), DEBUGGER_KEY_USED, or
 * DEBUGGER_KEY_LEFT (used, and the debugger has been left: redraw) or
 * DEBUGGER_KEY_EDIT (used: the caller opens the editor; the debugger stays). */
int  debugger_key(uint8_t code);
bool debugger_wants_repeat(uint8_t code);       /* a held key repeats (step, scroll) */

/* debugger_dirty() says a key or a step has made the screen out of date (the
 * caller also redraws it now and then while the Elf runs). debugger_draw()
 * draws it, writing only the rows whose text changed. */
bool debugger_dirty(void);
void debugger_draw(void);

#endif /* DEBUGGER_H */
