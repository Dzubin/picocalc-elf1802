/*
 * debugger_const.h - constants of the debugger view.
 *
 * Author: Thomas Dzubin
 */
#ifndef DEBUGGER_CONST_H
#define DEBUGGER_CONST_H

/* ------------------------------------------------------------------ */
/*  The debugger view (ESC menu, G): registers, a listing, a dump of     */
/*  memory, single-stepping and one breakpoint. The Elf is halted while  */
/*  it is on screen, except after G. Its keys are letters that the hex   */
/*  keypad does not use, so the keypad and IN still work.                */
/* ------------------------------------------------------------------ */
/* What debugger_key() returns. */
#define DEBUGGER_KEY_PASS   0       /* not the debugger's key                 */
#define DEBUGGER_KEY_USED   1       /* handled                                */
#define DEBUGGER_KEY_LEFT   2       /* handled, and the debugger was left     */
#define DEBUGGER_KEY_EDIT   3       /* handled: the caller opens the editor   */

#define DKEY_STEP           's'     /* run one instruction (SPACE does too)   */
#define DKEY_GO             'g'     /* run until the breakpoint, if any       */
#define DKEY_HALT           'h'     /* stop at the next instruction           */
#define DKEY_BREAK          'p'     /* breakpoint on the top line of the list */
#define DKEY_HOME           'w'     /* the list follows the PC again          */
#define DKEY_EDIT           't'     /* the text editor (the debugger stays)   */
#define DKEY_PAGE           16      /* LEFT and RIGHT move the list this far  */

#define DBG_ROW_TITLE        0
#define DBG_ROW_FLAGS        1      /* P X D DF Q IE T                        */
#define DBG_ROW_REGS         2      /* sixteen registers, four to a row       */
#define DBG_REG_ROWS         4
#define DBG_ROW_LIST         7      /* the disassembly                        */
#define DBG_LIST_LINES      13
#define DBG_ROW_DUMP        21      /* memory from the data pointer R(X)      */
#define DBG_DUMP_LINES       4
#define DBG_DUMP_BYTES       8      /* bytes to a line                        */
#define DBG_ROW_CYCLES      26
#define DBG_ROW_HELP        28      /* three lines                            */

#endif /* DEBUGGER_CONST_H */
