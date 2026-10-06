/*
 * mc6847.h - the Motorola MC6847 video display generator: 32 x 16 text,
 * semigraphics 4 and 6, and the eight graphics modes (64 x 64 up to 256 x 192).
 *
 * The chip reads display memory by itself (it counts addresses) and draws 192
 * lines of 256 pixels. Here it works a line at a time: the machine calls
 * mc6847_advance() with the processor cycles that went by, and at the end of
 * each of the 262 lines of the frame the chip records the mode pins as they
 * are then and the bytes that line shows. A machine that changes the mode
 * pins in the middle of a frame gets the change from that line on. Nothing is
 * drawn until it is asked for: mc6847_render_line() turns a recorded line into
 * 256 palette indices, and the lines that differ from the last time they were
 * drawn are marked, so a front end redraws only those.
 *
 * Portable ISO C with no machine dependencies.
 *
 * Author: Thomas Dzubin
 */
#ifndef MC6847_H
#define MC6847_H

#include <stdbool.h>
#include <stdint.h>

#include "mc6847_const.h"

typedef struct mc6847 mc6847_t;

struct mc6847 {
    /* what the machine sets */
    void    *ctx;                   /* passed back to read()                  */
    uint8_t (*read)(void *ctx, uint16_t addr);  /* display memory, by the chip's
                                       own address (0 up to 6143)            */
    uint8_t  mode;                  /* M6847_AG and the other mode pins       */

    /* the chip's position in the frame */
    int      line;                  /* 0 to 261                               */
    int      cycles;                /* processor cycles into the line         */
    uint32_t frames;                /* frames completed                       */

    /* the recorded display */
    uint8_t  rec_mode[M6847_HEIGHT];
    uint8_t  rec_data[M6847_HEIGHT][M6847_COLUMNS];
    bool     dirty[M6847_HEIGHT];   /* changed since mc6847_take_dirty()      */
};

/* Power-on: the mode pins low, the display empty and every line marked as
 * changed. The callback and ctx are left as the caller set them. */
void mc6847_reset(mc6847_t *v);

/* Let n processor cycles go by (one line is M6847_CYCLES_PER_LINE of them). */
void mc6847_advance(mc6847_t *v, int n);

/* Turn recorded display line y (0 to 191) into 256 palette indices (a y outside
 * that gives a black line). */
void mc6847_render_line(const mc6847_t *v, int y, uint8_t *out);

/* True if line y changed since it was last asked about; clears the mark (a y
 * outside 0 to 191 is never changed). */
bool mc6847_take_dirty(mc6847_t *v, int y);

/* The palette index of the border now. */
uint8_t mc6847_border(const mc6847_t *v);

/* The field sync output: high except for 32 lines at the bottom of the
 * display. */
bool mc6847_field_sync(const mc6847_t *v);

#endif /* MC6847_H */
