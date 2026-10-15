/*
 * terminal.h - a small text terminal in the top area of the screen, for the
 * characters that a program sends down the serial line: 40 columns by 16 rows,
 * scrolling, with carriage return, line feed, backspace, tab and form feed, and
 * the ANSI sequences for clear screen and cursor home. Drawing goes through
 * ui.h, and only the rows that changed are written.
 *
 * Author: Thomas Dzubin
 */
#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdint.h>

#include "terminal_const.h"

void terminal_clear(void);              /* a blank screen, cursor at the top left */
void terminal_put(uint8_t c);           /* a character arrives                    */
void terminal_forget(void);             /* the area was drawn over: redraw it all */
void terminal_draw(void);               /* draw the rows that changed             */

#endif /* TERMINAL_H */
