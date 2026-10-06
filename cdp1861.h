/*
 * cdp1861.h - the RCA CDP1861 "Pixie" video chip, kept in step with the CPU
 * one machine cycle at a time.
 *
 * The chip has no memory of its own. While the display is on it interrupts
 * the CPU just before the picture, then requests DMA: 8 bytes in a burst on
 * each of 128 lines, taken by the CPU from the memory R0 points at. A program
 * must set R0 in its interrupt routine and execute the right number of cycles
 * so that its own instructions fall between the bursts. Where a DMA byte lands
 * on the screen depends on which cycle of the line it was transferred in, so
 * a program that is a cycle out of step gets a shifted, gappy picture, as it
 * would on the real chip.
 *
 * Portable ISO C. Constants are in elf_const.h.
 *
 * Author: Thomas Dzubin
 */
#ifndef CDP1861_H
#define CDP1861_H

#include <stdbool.h>
#include <stdint.h>

#include "elf_const.h"

typedef struct {
    bool     on;                    /* display enabled (INP 1 on, OUT 1 off) */
    bool     int_active;            /* INTERRUPT is being held low           */
    uint16_t line;                  /* 0 to PIXIE_LINES_PER_FRAME - 1        */
    uint8_t  line_cycle;            /* 0 to PIXIE_CYCLES_PER_LINE - 1        */
    uint32_t frames;                /* pictures completed, for the screen    */
    uint8_t  bits[PIXIE_DISPLAY_LINES][PIXIE_BYTES_PER_LINE];
                                    /* the picture, one bit a pixel, the
                                       leftmost pixel the top bit of byte 0   */
} pixie_t;

void pixie_init(pixie_t *v);

void pixie_display_on(pixie_t *v);
void pixie_display_off(pixie_t *v);

/* The chip's outputs for the machine cycle that is about to run. */
bool pixie_int_request(const pixie_t *v);   /* INTERRUPT is low              */
bool pixie_dma_request(const pixie_t *v);   /* DMA-OUT is low                */
bool pixie_efx_low(const pixie_t *v);       /* EFX is low (EF1 reads 0)      */

/* One DMA byte arrives (an S2 cycle). */
void pixie_dma_byte(pixie_t *v, uint8_t data);

/* A machine cycle has run: move the line and cycle counters on. */
void pixie_cycle_done(pixie_t *v);

#endif /* CDP1861_H */
