/*
 * terminal_const.h - constants of the terminal screen (terminal.c): its size,
 * where it goes and its colours.
 *
 * Author: Thomas Dzubin
 */
#ifndef TERMINAL_CONST_H
#define TERMINAL_CONST_H

#define TERM_COLS           40      /* the whole width of the screen           */
#define TERM_ROWS           16      /* text rows in the top area               */
#define TERM_CURSOR         '_'
#define COL_TERM_TEXT       COL_GREEN

/* Control characters it understands; an escape sequence (ESC [ ... letter) is
 * skipped, apart from clear screen (ESC [ 2 J) and cursor home (ESC [ H). */
#define TERM_BS             0x08
#define TERM_TAB            0x09
#define TERM_LF             0x0A
#define TERM_FF             0x0C
#define TERM_CR             0x0D
#define TERM_ESC            0x1B
#define TERM_TAB_WIDTH      8

#endif /* TERMINAL_CONST_H */
