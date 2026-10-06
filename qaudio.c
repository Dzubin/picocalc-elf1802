/*
 * qaudio.c - Q as sound. See qaudio.h.
 *
 * The sample clock is a Bresenham-style accumulator: each machine cycle adds
 * QAUDIO_RATE, and every time the sum passes the machine-cycle rate a sample is
 * finished, so there are exactly QAUDIO_RATE samples for every second of
 * emulated time with no drift. A sample is the fraction of its cycles that Q
 * was high (a box filter, which also keeps very fast Q pulses from aliasing
 * into whistles), less the slow average of Q (the DC blocker).
 *
 * Author: Thomas Dzubin
 */
#include <string.h>

#include "qaudio.h"

void qaudio_init(qaudio_t *a, uint32_t clock_hz)
{
    memset(a, 0, sizeof *a);
    a->clock_hz = clock_hz;
}

static void push_sample(qaudio_t *a, int16_t sample)
{
    if (a->count >= QAUDIO_FIFO)
        return;                             /* nobody is listening: drop it */
    a->fifo[(a->head + a->count) % QAUDIO_FIFO] = sample;
    a->count++;
}

/* The sample for a stretch of "length" cycles of which "high" had Q high. A
 * sample is about ten cycles, so the product fits 32 bits (a 64-bit divide is
 * a slow library call on the chips this runs on). */
static int16_t finish_sample(qaudio_t *a)
{
    int32_t level = (int32_t)((a->high * QAUDIO_FULL) / a->length);
    int32_t swing;

    a->dc += ((level << 8) - a->dc) >> QAUDIO_DC_SHIFT;
    swing = level - (a->dc >> 8);
    return (int16_t)((swing * QAUDIO_VOLUME) >> 8);
}

void qaudio_cycle(qaudio_t *a, bool q)
{
    if (q)
        a->high++;
    a->length++;
    a->phase += QAUDIO_RATE;
    if (a->phase >= a->clock_hz) {
        a->phase -= a->clock_hz;
        push_sample(a, finish_sample(a));
        a->high = 0;
        a->length = 0;
    }
}

/* Author: Thomas Dzubin */
void qaudio_cycles(qaudio_t *a, bool q, uint32_t count)
{
    while (count > 0) {
        /* cycles until the sample clock finishes the sample (at least one) */
        uint32_t to_sample = (a->clock_hz - a->phase + QAUDIO_RATE - 1) / QAUDIO_RATE;
        uint32_t n = count < to_sample ? count : to_sample;

        if (q)
            a->high += n;
        a->length += n;
        a->phase += n * QAUDIO_RATE;
        count -= n;
        if (a->phase >= a->clock_hz) {
            a->phase -= a->clock_hz;
            push_sample(a, finish_sample(a));
            a->high = 0;
            a->length = 0;
        }
    }
}

uint32_t qaudio_read(qaudio_t *a, int16_t *out, uint32_t max)
{
    uint32_t n = a->count < max ? a->count : max;
    uint32_t i;

    for (i = 0; i < n; i++)
        out[i] = a->fifo[(a->head + i) % QAUDIO_FIFO];
    a->head = (uint16_t)((a->head + n) % QAUDIO_FIFO);
    a->count = (uint16_t)(a->count - n);
    return n;
}

void qaudio_flush(qaudio_t *a)
{
    a->head = 0;
    a->count = 0;
}
