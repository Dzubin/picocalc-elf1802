/*
 * picture_const.h - constants of the 1861 picture on the screen (picture.c):
 * where it goes and how big its pixels are, in the top area of the screen.
 *
 * Author: Thomas Dzubin
 */
#ifndef PICTURE_CONST_H
#define PICTURE_CONST_H

#include "elf_const.h"

#define COL_PICTURE RGB(235, 235, 235)       /* lit pixels of the 1861 image */
#define PICTURE_X          0
#define PICTURE_Y          16       /* 128 lines centred in TOP_AREA_HEIGHT  */
#define PICTURE_SCALE_X    5        /* 64 * 5 = 320 pixels across            */
#define PICTURE_SCALE_Y    1        /* 128 lines, one pixel each             */
#define PICTURE_WIDTH      (PIXIE_PIXELS_PER_LINE * PICTURE_SCALE_X)
#define PICTURE_BAND_LINES 16       /* lines drawn per blit (and per compare) */
#define PICTURE_BANDS      (PIXIE_DISPLAY_LINES / PICTURE_BAND_LINES)

#endif /* PICTURE_CONST_H */
