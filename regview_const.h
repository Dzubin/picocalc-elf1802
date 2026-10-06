/*
 * regview_const.h - constants of the register read-out (regview.c) that fills
 * the top of the screen when the Elf has no 1861 video chip: the rows of text
 * in the top 16 rows of the screen.
 *
 * Author: Thomas Dzubin
 */
#ifndef REGVIEW_CONST_H
#define REGVIEW_CONST_H

#define REGVIEW_LAST_ROW     15     /* the top area is rows 0 to 15           */
#define REGVIEW_ROW_TITLE     1
#define REGVIEW_ROW_FLAGS     3     /* P X D DF Q IE T                        */
#define REGVIEW_ROW_REGS      5     /* sixteen registers, four to a row       */
#define REGVIEW_REG_ROWS      4
#define REGVIEW_ROW_NEXT     10     /* the instruction about to run           */
#define REGVIEW_ROW_CYCLES   12
#define REGVIEW_ROW_HINT     14

#endif /* REGVIEW_CONST_H */
