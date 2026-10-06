/*
 * cpu6803.h - a Motorola MC6803 microprocessor (the 6800 with a few more
 * instructions and, on the chip: 128 bytes of RAM, four ports, a 16-bit
 * free-running timer with output compare and input capture, and a serial
 * interface). It is the processor of the TRS-80 MC-10.
 *
 * cpu6803_step() runs one whole instruction (or an interrupt entry, or one idle
 * cycle of WAI) and returns the machine cycles (E clocks) it took, from the data
 * sheet's table; the on-chip timer is advanced by those cycles, so the caller
 * keeps the rest of the machine in step by what is returned. The on-chip
 * registers and RAM answer at their addresses (0x00 to 0x14 and 0x80 to 0xFF);
 * every other address goes to the bus callbacks. The ports go to callbacks too.
 *
 * Portable ISO C with no machine dependencies.
 *
 * Author: Thomas Dzubin
 */
#ifndef CPU6803_H
#define CPU6803_H

#include <stdbool.h>
#include <stdint.h>

#include "cpu6803_const.h"

typedef struct cpu6803 cpu6803_t;

struct cpu6803 {
    /* registers */
    uint8_t  a, b;                  /* the accumulators (D is A:B)            */
    uint16_t x, sp, pc;
    uint8_t  cc;                    /* M68_CC_*, six bits                     */
    bool     wai;                   /* stopped by WAI, waiting for an interrupt */
    bool     trapped;               /* the last opcode was illegal            */
    bool     defer_int;             /* no interrupt before the next instruction
                                       (after CLI or TAP)                     */

    /* input pins, set by the caller */
    bool     irq;                   /* IRQ1 is asserted (low), a level        */
    bool     nmi_pending;           /* set by cpu6803_nmi() on the falling edge */

    /* the bus */
    void    *ctx;                   /* passed back to every callback          */
    uint8_t (*mem_read)(void *ctx, uint16_t addr);
    void    (*mem_write)(void *ctx, uint16_t addr, uint8_t data);
    /* port is 1 to 4. port_in gives the level of the pins; port_out is told
     * the level of all eight pins (a pin that is an input floats high) and
     * the direction register, after every change. */
    uint8_t (*port_in)(void *ctx, int port);
    void    (*port_out)(void *ctx, int port, uint8_t pins, uint8_t ddr);

    /* on-chip */
    uint8_t  ram[M68_RAM_SIZE];
    bool     ram_enabled;           /* the RAM control register's RAME        */
    uint8_t  ddr[4], data[4];       /* ports 1 to 4                           */
    uint8_t  tcsr;
    uint8_t  pending;               /* TCSR flags set since TCSR was last read */
    uint16_t counter, ocr, icr;
    uint8_t  counter_low;           /* the low byte kept when the high is read */
    bool     counter_low_held;
    bool     tin;                   /* the last level on the capture pin      */
    uint8_t  rmcr, trcsr, rdr, rcr, p3csr;

    uint32_t cycles;                /* machine cycles run (wraps)             */
};

/* Power-on: everything cleared, then reset. The callbacks and ctx are left
 * as the caller set them; the vector is read through mem_read. */
void cpu6803_reset(cpu6803_t *c);

/* Run one instruction (or interrupt entry, or one idle cycle in WAI) and
 * return the cycles it took. */
int cpu6803_step(cpu6803_t *c);

/* The NMI pin: call with true as it falls (a falling edge makes one NMI). */
void cpu6803_nmi(cpu6803_t *c, bool asserted);

/* The input capture pin (P20): an edge captures the counter when it is the
 * edge TCSR's IEDG selects (rising when set). */
void cpu6803_input_capture(cpu6803_t *c, bool level);

/* Read memory as the CPU sees it (the on-chip registers and RAM first, then the
 * bus), without the side effects of reading a timer register. */
uint8_t cpu6803_peek(cpu6803_t *c, uint16_t addr);

#endif /* CPU6803_H */
