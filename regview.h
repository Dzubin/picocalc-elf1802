/*
 * regview.h - the register read-out in the top of the screen for an Elf with no
 * 1861 video chip (the basic Elf: an 1802, RAM, keypad, displays and the Q LED).
 * There is no picture to show, so the area shows the CPU: P, X, D, DF, Q, IE, T,
 * the sixteen registers, the next instruction and the cycle count.
 *
 * Author: Thomas Dzubin
 */
#ifndef REGVIEW_H
#define REGVIEW_H

#include "elf.h"
#include "regview_const.h"

/* The area was cleared or drawn over: draw every row afresh. */
void regview_forget(void);

/* Draw the read-out; only the rows whose text changed are written. */
void regview_update(const elf_t *m);

#endif /* REGVIEW_H */
