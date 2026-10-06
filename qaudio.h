/*
 * qaudio.h - the Elf's Q line as real sound. The board calls qaudio_cycle()
 * once for every machine cycle with the level of Q; the level is averaged over
 * each sample (about ten cycles), a DC blocker takes out the steady part, and
 * the samples wait in a FIFO until the front end collects them and plays them.
 * So a tone, a PCM-style pulse train or a program that wiggles Q at any rate is
 * heard as the Elf's speaker would play it, not as an estimate of a pitch.
 *
 * Portable ISO C. Constants (QAUDIO_*) are in elf_const.h.
 *
 * Author: Thomas Dzubin
 */
#ifndef QAUDIO_H
#define QAUDIO_H

#include <stdbool.h>
#include <stdint.h>

#include "elf_const.h"

typedef struct {
    int16_t  fifo[QAUDIO_FIFO];     /* finished samples, signed, 0 = silence */
    uint16_t head;                  /* next sample to hand out               */
    uint16_t count;                 /* samples waiting                       */
    uint32_t phase;                 /* where in the sample clock we are      */
    uint32_t high;                  /* cycles with Q high in this sample     */
    uint32_t length;                /* cycles in this sample so far          */
    int32_t  dc;                    /* the average level, 8 fraction bits    */
    uint32_t clock_hz;              /* machine cycles a second               */
} qaudio_t;

/* Set up for a machine that runs clock_hz machine cycles a second. */
void qaudio_init(qaudio_t *a, uint32_t clock_hz);

/* One machine cycle went by with Q at this level. A sample that fills up when
 * the FIFO is full is dropped. */
void qaudio_cycle(qaudio_t *a, bool q);

/* count machine cycles went by with Q at this level: the same samples as
 * count calls of qaudio_cycle(), for a machine whose cycles come a whole
 * instruction at a time. */
void qaudio_cycles(qaudio_t *a, bool q, uint32_t count);

/* Copy up to max waiting samples to out; returns how many were taken. */
uint32_t qaudio_read(qaudio_t *a, int16_t *out, uint32_t max);

/* Throw away the samples waiting (the machine was stopped, a menu was open). */
void qaudio_flush(qaudio_t *a);

#endif /* QAUDIO_H */
