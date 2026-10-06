/*
 * cpu1802.h - an RCA CDP1802 (COSMAC) microprocessor, one machine cycle at a
 * time.
 *
 * The chip does everything in 8-clock machine cycles: S0 fetches an
 * instruction, S1 executes it (a second S1 for long branches, long skips and
 * NOP), S2 is a DMA transfer, S3 is the interrupt response. cpu1802_cycle()
 * runs exactly one of those and says which, so a video chip or a front panel
 * can be kept in step with the processor to the cycle.
 *
 * The caller sets the input lines (mode, flags, requests) before each call,
 * the way the pins would be at that moment, and supplies the bus through the
 * callbacks. The file is portable ISO C with no machine dependencies.
 *
 * Author: Thomas Dzubin
 */
#ifndef CPU1802_H
#define CPU1802_H

#include <stdbool.h>
#include <stdint.h>

typedef struct cpu1802 cpu1802_t;

struct cpu1802 {
    /* registers */
    uint16_t r[16];                 /* the sixteen scratchpad registers      */
    uint8_t  d;                     /* accumulator                           */
    uint8_t  t;                     /* holds X and P after an interrupt      */
    uint8_t  x, p;                  /* which registers are data pointer, PC  */
    uint8_t  i, n;                  /* the two halves of the opcode          */
    bool     df;                    /* carry / borrow                        */
    bool     q;                     /* the Q output                          */
    bool     ie;                    /* interrupt enable                      */

    /* input pins, set by the caller before every cycle */
    uint8_t  mode;                  /* CPU_MODE_*                            */
    uint8_t  ef;                    /* EF pins, 1 = high (CPU_EF1 to CPU_EF4);
                                       a flag is "true" (B1 to B4) when LOW   */
    bool     int_req;               /* INTERRUPT is asserted (low)           */
    bool     dma_in_req;            /* DMA-IN is asserted                    */
    bool     dma_out_req;           /* DMA-OUT is asserted                   */

    /* the bus */
    void    *ctx;                   /* passed back to every callback         */
    uint8_t (*mem_read)(void *ctx, uint16_t addr);
    void    (*mem_write)(void *ctx, uint16_t addr, uint8_t data);
    uint8_t (*io_in)(void *ctx, uint8_t port);              /* INP           */
    void    (*io_out)(void *ctx, uint8_t port, uint8_t data); /* OUT         */
    uint8_t (*dma_in)(void *ctx);                           /* data for DMA-IN */
    void    (*dma_out)(void *ctx, uint8_t data);            /* DMA-OUT byte  */

    /* bookkeeping */
    uint32_t cycles;                /* machine cycles run (wraps)            */
    uint8_t  clocks;                /* clock pulses the last cycle took      */
    uint8_t  phase;                 /* where in the cycle sequence we are    */
};

/* Power-on: registers cleared, held in reset. The bus callbacks and ctx are
 * left as the caller set them. */
void cpu1802_power_on(cpu1802_t *c);

/* Run one machine cycle for the pins as they are now. Returns CPU_CYCLE_S0,
 * S1, S2, S3 or INIT, or CPU_CYCLE_NONE when no clock pulses went by (in
 * reset, in pause, or idle in load mode with nothing requested). */
int cpu1802_cycle(cpu1802_t *c);

#endif /* CPU1802_H */
