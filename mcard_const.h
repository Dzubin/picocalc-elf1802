/*
 * mcard_const.h - constants of the 1802 Membership Card (mcard.c), Lee Hart's
 * small COSMAC board, as described in his Rev L manual: the clock, the memory
 * map, the I/O ports and the front panel's controls.
 *
 * Author: Thomas Dzubin
 */
#ifndef MCARD_CONST_H
#define MCARD_CONST_H

#include "elf_const.h"          /* CPU_MODE_*, CPU_EF*, CPU_CYCLE_*       */

#define MCARD_CLOCK_HZ          4000000UL   /* a 4 MHz ceramic resonator (Rev K on) */
#define MCARD_CYCLES_PER_SEC    (MCARD_CLOCK_HZ / CPU_CLOCKS_PER_CYCLE)

/* The memory map: 32K of RAM at U8 from 0000 and the optional chip at U2 (ROM or
 * RAM) from 8000, as the standard jumpers on P6 set it. */
#define MCARD_RAM_SIZE          0x8000
#define MCARD_ROM_BASE          0x8000
#define MCARD_ROM_SIZE          0x8000      /* the space for ROM images from 8000 (rom.c) */
#define MCARD_MONITOR_MAX       0x1000      /* the built-in monitor: the most it can be  */
#define MCARD_UNMAPPED_READ     0x00        /* what the bus gives where nothing is */

/* The I/O: the data switches are read by INP 4 and the data LEDs and the hex
 * display show OUT 4 (the manual names OUT 4; the input port is the matching
 * one, which is not confirmed). */
#define MCARD_PORT_DATA         4

/* The flag lines: EF4 is the IN button (low while pressed), EF3 is the serial
 * input (low is a 0 bit or the start bit); EF1 and EF2 are free. */
#define MCARD_EF_SERIAL         CPU_EF3
#define MCARD_EF_IN_BUTTON      CPU_EF4
#define MCARD_EF_FREE           (CPU_EF1 | CPU_EF2)

#define MCARD_DEFAULT_BAUD      4800        /* the manual: 4800, 8 bits, no parity, 1 stop */

#endif /* MCARD_CONST_H */
