/*
 * vdu_const.h - constants of the VDU on the screen (vdu.c): where it goes, the
 * size of a cell and the colours of its block graphics.
 *
 * Author: Thomas Dzubin
 */
#ifndef VDU_CONST_H
#define VDU_CONST_H

#define VDU_X              32
#define VDU_Y              0
#define VDU_CELL_W         8
#define VDU_CELL_H         10
#define VDU_DRAW_MS        20       /* how often changed cells are redrawn   */

/* The block graphics colours, from bits 4 and 5 of a cell: green, yellow,
 * blue and red; text is green. */
#define VDU_COLOURS { COL_GREEN, COL_YELLOW, RGB(90, 120, 255), COL_RED }
#define COL_VDU_TEXT        COL_GREEN

#endif /* VDU_CONST_H */
