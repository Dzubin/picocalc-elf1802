/*
 * machine.h - what the shell (the main loop, the pacing, the menus) needs to know
 * about a machine to run it: its name, how many machine cycles a second it runs,
 * and how to run some of them. A machine is a processor, a memory map and its
 * devices (the Elf is an 1802, the memory map of elf.c, and the 1861 and the
 * keypad); the shell asks it for cycles in step with real time and draws what
 * it shows. Portable ISO C.
 *
 * Author: Thomas Dzubin
 */
#ifndef MACHINE_H
#define MACHINE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct machine_s {
    const char *name;                   /* "Elf II"                                */
    uint32_t    clock_hz;               /* machine cycles a second at full speed   */

    /* Run up to cycles machine cycles of the machine self; returns how many ran,
     * which is fewer if no time was passing (the machine was held in RESET, for
     * instance). */
    uint32_t  (*run)(void *self, uint32_t cycles);

    /* Optional (NULL if the machine cannot): like run, but stop when the next
     * instruction to be fetched is at address (never before one instruction has
     * run); *hit says whether it stopped for that. */
    uint32_t  (*run_to)(void *self, uint32_t cycles, uint16_t address, bool *hit);
} machine_t;

#endif /* MACHINE_H */
