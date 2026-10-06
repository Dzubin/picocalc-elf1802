/*
 * cpuz80.h - the Zilog Z80 CPU core, as a skeleton to build on. One call of
 * cpuz80_step() runs one instruction (or takes an interrupt, or lets a halted
 * processor run one NOP) and returns the T-states it took, from the data sheet.
 *
 * What runs now is the 8080's instruction set as the Z80 executes it: the
 * unprefixed opcodes except JR, DJNZ, EX AF,AF' and EXX, with the Z80's flags
 * (including the undocumented bits 5 and 3), its T-state counts, the R register,
 * HALT, EI's one instruction delay, and the interrupts (NMI, and the maskable one
 * in modes 0, 1 and 2). An opcode that is not run yet (those four, and the CB, DD,
 * ED and FD groups) is taken as a one-byte NOP and sets `unsupported`, so a
 * program that needs it is easy to spot; they are the next things to add
 * (cpuz80_const.h has the table to fill in, isa_z80.c describes every opcode).
 *
 * Memory goes through mem_read and mem_write; the I/O ports through io_in and
 * io_out, with the 16-bit address the Z80 puts on its bus (the accumulator in the
 * high byte for IN A,(n) and OUT (n),A). Portable ISO C, no platform headers.
 *
 * Author: Thomas Dzubin
 */
#ifndef CPUZ80_H
#define CPUZ80_H

#include <stdbool.h>
#include <stdint.h>

#include "cpuz80_const.h"

typedef struct cpuz80 {
    uint8_t  a, f, b, c, d, e, h, l;        /* the main registers                  */
    uint8_t  a2, f2, b2, c2, d2, e2, h2, l2;/* the alternate set (not used yet)    */
    uint16_t ix, iy, sp, pc;
    uint8_t  i, r;                          /* interrupt vector base, refresh      */
    bool     iff1, iff2;                    /* interrupt enable flip-flops         */
    uint8_t  im;                            /* interrupt mode, 0 to 2              */
    bool     halted;
    bool     ei_delay;                      /* EI was the last instruction: no maskable
                                               interrupt before the next one runs   */

    /* the pins and the data bus the machine drives */
    bool     irq;                           /* INT is asserted (a level)           */
    uint8_t  irq_data;                      /* what the device puts on the bus     */
    bool     nmi_pending;

    /* the last instruction was one this core does not run yet */
    bool     unsupported;
    uint8_t  unsupported_op;

    void    *ctx;                           /* what the callbacks are given        */
    uint8_t (*mem_read)(void *ctx, uint16_t addr);
    void    (*mem_write)(void *ctx, uint16_t addr, uint8_t data);
    uint8_t (*io_in)(void *ctx, uint16_t port);
    void    (*io_out)(void *ctx, uint16_t port, uint8_t data);

    uint32_t cycles;                        /* T-states since reset                */
} cpuz80_t;

/* Reset the processor (the callbacks and ctx are kept). */
void cpuz80_reset(cpuz80_t *cpu);

/* Run one instruction, or take an interrupt; returns the T-states it took. */
int cpuz80_step(cpuz80_t *cpu);

/* The NMI pin: an edge asks for the interrupt (asserted true), which is taken
 * before the next instruction whatever the interrupt enable is. */
void cpuz80_nmi(cpuz80_t *cpu, bool asserted);

#endif /* CPUZ80_H */
