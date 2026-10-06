/*
 * vdu.h - the VDU on the PicoCalc's screen: the 32 x 16 character cells in the
 * top area, text or 2 x 2 block graphics, drawn cell by cell and only the cells
 * that changed since the last time.
 *
 * Author: Thomas Dzubin
 */
#ifndef VDU_H
#define VDU_H

#include "elf.h"
#include "vdu_const.h"

/* The VDU area was cleared or drawn over: draw every cell afresh. */
void vdu_forget(void);

/* Draw the cells of the VDU's video RAM that changed. */
void vdu_draw(const elf_t *m);

#endif /* VDU_H */
