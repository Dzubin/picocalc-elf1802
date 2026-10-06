/*
 * vdu.c - the VDU on the screen. See vdu.h.
 *
 * Author: Thomas Dzubin
 */
#include <stdbool.h>
#include <stdint.h>

#include "lcd.h"          /* lcd_putc(), lcd_solid_rectangle(), RGB()         */

#include "elf1802_const.h"  /* the colours                                    */
#include "vdu.h"
#include "vdu_const.h"

static uint8_t shown[VDU_CELLS];        /* what each cell shows now           */
static bool    valid;                   /* shown[] is what is on the screen   */

void vdu_forget(void)
{
    valid = false;
}

/* One cell of the VDU: text from the font, or four blocks.
 *
 * Author: Thomas Dzubin */
static void draw_cell(int index, uint8_t v)
{
    static const uint16_t colours[4] = VDU_COLOURS;
    int col = index % VDU_COLS;
    int row = index / VDU_COLS;
    int x = VDU_X + col * VDU_CELL_W;
    int y = VDU_Y + row * VDU_CELL_H;

    if (v & VDU_BIT_BLOCKS) {
        uint16_t colour = colours[(v >> VDU_COLOUR_SHIFT) & VDU_COLOUR_MASK];
        int q;

        lcd_solid_rectangle(COL_BG, (uint16_t)x, (uint16_t)y, VDU_CELL_W, VDU_CELL_H);
        for (q = 0; q < 4; q++)     /* top left, top right, bottom left, bottom right */
            if (v & (8 >> q))
                lcd_solid_rectangle(colour,
                                    (uint16_t)(x + (q & 1) * (VDU_CELL_W / 2)),
                                    (uint16_t)(y + (q >> 1) * (VDU_CELL_H / 2)),
                                    VDU_CELL_W / 2, VDU_CELL_H / 2);
    } else {
        unsigned ch = v & VDU_CODE_MASK;
        bool inverse = (v & VDU_BIT_INVERSE) != 0;

        if (ch < 32)
            ch += 64;               /* codes 0 to 31 are @, A to Z and the next five */
        lcd_set_foreground(inverse ? COL_BG : COL_VDU_TEXT);
        lcd_set_background(inverse ? COL_VDU_TEXT : COL_BG);
        lcd_putc((uint8_t)(VDU_X / VDU_CELL_W + col), (uint8_t)row, (uint8_t)ch);
        lcd_set_background(COL_BG);
    }
}

void vdu_draw(const elf_t *m)
{
    int i;

    for (i = 0; i < VDU_CELLS; i++) {
        if (valid && shown[i] == m->vdu[i])
            continue;
        shown[i] = m->vdu[i];
        draw_cell(i, m->vdu[i]);
    }
    valid = true;
}
