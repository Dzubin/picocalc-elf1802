/*
 * cdp1861.c - the CDP1861 video chip. See cdp1861.h.
 *
 * Timing, in machine cycles (14 to a line, 262 lines to a frame):
 *
 *   line 76 to 79    EFX low (the four lines before the picture)
 *   line 78 to 79    INTERRUPT low, 28 cycles, only while the display is on
 *   line 80 to 207   the picture: on each line, cycles 2 to 9 request DMA
 *   line 204 to 207  EFX low again (the last four lines of the picture)
 *
 * The interrupt is taken at the start of line 78 when the CPU is on an
 * instruction boundary, so with only two-cycle instructions before it the
 * first DMA byte comes 29 cycles after the interrupt cycle, which is the
 * figure the data sheet and the standard Elf display routine are built on.
 *
 * Author: Thomas Dzubin
 */
#include <string.h>

#include "cdp1861.h"

void pixie_init(pixie_t *v)
{
    memset(v, 0, sizeof *v);
}

void pixie_display_on(pixie_t *v)
{
    v->on = true;
}

void pixie_display_off(pixie_t *v)
{
    v->on = false;
    v->int_active = false;          /* turning it off drops the interrupt */
}

bool pixie_int_request(const pixie_t *v)
{
    return v->on && v->int_active;
}

bool pixie_dma_request(const pixie_t *v)
{
    return v->on &&
           v->line >= PIXIE_DISPLAY_FIRST && v->line < PIXIE_DISPLAY_END &&
           v->line_cycle >= PIXIE_DMA_FIRST_CYCLE &&
           v->line_cycle < PIXIE_DMA_FIRST_CYCLE + PIXIE_BYTES_PER_LINE;
}

bool pixie_efx_low(const pixie_t *v)
{
    return (v->line >= PIXIE_EFX_TOP_FIRST && v->line < PIXIE_DISPLAY_FIRST) ||
           (v->line >= PIXIE_EFX_BOTTOM_FIRST && v->line < PIXIE_DISPLAY_END);
}

void pixie_dma_byte(pixie_t *v, uint8_t data)
{
    int row = (int)v->line - PIXIE_DISPLAY_FIRST;
    int slot = (int)v->line_cycle - PIXIE_DMA_FIRST_CYCLE;

    if (row >= 0 && row < PIXIE_DISPLAY_LINES &&
        slot >= 0 && slot < PIXIE_BYTES_PER_LINE)
        v->bits[row][slot] = data;
}

/* Author: Thomas Dzubin */
void pixie_cycle_done(pixie_t *v)
{
    if (++v->line_cycle >= PIXIE_CYCLES_PER_LINE) {
        v->line_cycle = 0;
        if (++v->line >= PIXIE_LINES_PER_FRAME)
            v->line = 0;

        if (v->line == PIXIE_INT_FIRST_LINE && v->on)
            v->int_active = true;
        else if (v->line == PIXIE_INT_END_LINE)
            v->int_active = false;

        if (v->line == PIXIE_DISPLAY_END && v->on)
            v->frames++;
    }

    /* A new burst is about to start: bytes that do not arrive show as blank. */
    if (v->on && v->line_cycle == PIXIE_DMA_FIRST_CYCLE &&
        v->line >= PIXIE_DISPLAY_FIRST && v->line < PIXIE_DISPLAY_END)
        memset(v->bits[v->line - PIXIE_DISPLAY_FIRST], 0, PIXIE_BYTES_PER_LINE);
}
