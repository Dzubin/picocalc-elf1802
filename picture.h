/*
 * picture.h - the 1861's picture on the PicoCalc's screen: the 64 x 128 pixels
 * drawn five wide in the top area, a band of lines at a time, and only the bands
 * that changed since the last time. With the 1861 off the area goes black.
 *
 * Author: Thomas Dzubin
 */
#ifndef PICTURE_H
#define PICTURE_H

#include <stdbool.h>

#include "elf.h"
#include "picture_const.h"

/* The picture area was cleared or drawn over: draw everything afresh. With
 * draw_now the next picture_update() draws even if nothing changed. */
void picture_forget(const elf_t *m, bool draw_now);

/* Draw the picture if the 1861 was turned on or off or has finished a frame. */
void picture_update(const elf_t *m);

#endif /* PICTURE_H */
