/*
 * picture.c - the 1861's picture on the screen. See picture.h.
 *
 * Author: Thomas Dzubin
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "lcd.h"          /* lcd_blit(), lcd_solid_rectangle(), RGB()         */

#include "elf1802_const.h"  /* COL_BG                                         */
#include "picture.h"
#include "picture_const.h"

static uint16_t band_pixels[PICTURE_WIDTH * PICTURE_BAND_LINES * PICTURE_SCALE_Y];
static uint8_t  shown_bits[PIXIE_DISPLAY_LINES][PIXIE_BYTES_PER_LINE];
static bool     lit;                    /* the area holds an image            */
static uint32_t frames_drawn;

void picture_forget(const elf_t *m, bool draw_now)
{
    memset(shown_bits, 0, sizeof shown_bits);
    lit = false;
    frames_drawn = draw_now ? m->video.frames - 1 : m->video.frames;
}

/* Draw the 1861 picture, a band of lines at a time and only the bands that
 * changed since the last time. With the display off the area goes black.
 *
 * Author: Thomas Dzubin */
static void draw(const pixie_t *v)
{
    int band, line, byte, bit, sx, dy;

    if (!v->on) {
        if (lit) {
            lcd_solid_rectangle(COL_BG, PICTURE_X, PICTURE_Y, PICTURE_WIDTH,
                                PIXIE_DISPLAY_LINES * PICTURE_SCALE_Y);
            memset(shown_bits, 0, sizeof shown_bits);
            lit = false;
        }
        return;
    }

    lit = true;
    for (band = 0; band < PICTURE_BANDS; band++) {
        int first = band * PICTURE_BAND_LINES;
        uint16_t *out = band_pixels;

        if (memcmp(v->bits[first], shown_bits[first],
                   PICTURE_BAND_LINES * PIXIE_BYTES_PER_LINE) == 0)
            continue;
        memcpy(shown_bits[first], v->bits[first],
               PICTURE_BAND_LINES * PIXIE_BYTES_PER_LINE);

        for (line = first; line < first + PICTURE_BAND_LINES; line++) {
            uint16_t *row = out;

            for (byte = 0; byte < PIXIE_BYTES_PER_LINE; byte++)
                for (bit = 7; bit >= 0; bit--) {
                    uint16_t colour = ((v->bits[line][byte] >> bit) & 1) ?
                                      COL_PICTURE : COL_BG;
                    for (sx = 0; sx < PICTURE_SCALE_X; sx++)
                        *out++ = colour;
                }
            for (dy = 1; dy < PICTURE_SCALE_Y; dy++) {
                memcpy(out, row, PICTURE_WIDTH * sizeof *out);
                out += PICTURE_WIDTH;
            }
        }
        lcd_blit(band_pixels, PICTURE_X, (uint16_t)(PICTURE_Y + first * PICTURE_SCALE_Y),
                 PICTURE_WIDTH, PICTURE_BAND_LINES * PICTURE_SCALE_Y);
    }
}

void picture_update(const elf_t *m)
{
    if (m->video.on == lit && m->video.frames == frames_drawn)
        return;
    frames_drawn = m->video.frames;
    draw(&m->video);
}
