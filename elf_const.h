/*
 * elf_const.h - the constants of the emulated computer: the CDP1802 CPU, the
 * CDP1861 video chip and the Elf board around them (memory, ports, flags).
 * Nothing here depends on the PicoCalc, so the emulator files (cpu1802.c,
 * cdp1861.c, elf.c) and their tests build on any machine. Screen positions,
 * colours and key assignments are in elf1802_const.h instead.
 *
 * Author: Thomas Dzubin
 */
#ifndef ELF_CONST_H
#define ELF_CONST_H

/* ------------------------------------------------------------------ */
/*  Clock                                                               */
/* ------------------------------------------------------------------ */

/* The Elf runs its 1802 from the 3.579545 MHz colour-burst crystal divided
 * by two, which is also what makes the 1861's line rate match a TV. */
#define ELF_CLOCK_HZ            1789772UL
#define CPU_CLOCKS_PER_CYCLE    8        /* clock pulses in a machine cycle  */
#define CPU_INIT_CLOCKS         9        /* the cycle after a reset has 9    */
#define ELF_CYCLES_PER_SEC      (ELF_CLOCK_HZ / CPU_CLOCKS_PER_CYCLE)

/* ------------------------------------------------------------------ */
/*  The 1802: control modes and the kinds of machine cycle              */
/* ------------------------------------------------------------------ */

/* The value is (CLEAR << 1) | WAIT, with 1 meaning the pin is high. */
#define CPU_MODE_LOAD           0        /* CLEAR low,  WAIT low             */
#define CPU_MODE_RESET          1        /* CLEAR low,  WAIT high            */
#define CPU_MODE_PAUSE          2        /* CLEAR high, WAIT low             */
#define CPU_MODE_RUN            3        /* CLEAR high, WAIT high            */

/* What cpu1802_cycle() reports for the machine cycle it just ran. */
#define CPU_CYCLE_NONE          0        /* no cycle: reset, pause, or idle
                                            in load mode (no clock pulses)   */
#define CPU_CYCLE_S0            1        /* fetch                            */
#define CPU_CYCLE_S1            2        /* execute                          */
#define CPU_CYCLE_S2            3        /* DMA                              */
#define CPU_CYCLE_S3            4        /* interrupt                        */
#define CPU_CYCLE_INIT          5        /* initialization cycle after reset */

/* Where cpu1802_cycle() is in the machine-cycle sequence. */
#define CPU_PHASE_RESET         0        /* held in reset (next: init cycle) */
#define CPU_PHASE_FIRST         1        /* just after init: S0, S1 or S2    */
#define CPU_PHASE_NEXT          2        /* a decision point: S0, S2 or S3   */
#define CPU_PHASE_EXEC          3        /* S1 follows the fetch             */
#define CPU_PHASE_EXEC2         4        /* second S1 of a long instruction  */
#define CPU_PHASE_IDLE          5        /* executing IDL                    */
#define CPU_PHASE_LOAD          6        /* idle in load mode                */

/* The long-group opcodes C0 to CF that are branches (the rest are skips and
 * NOP): one bit per low nibble, set for 0 to 3 and 9 to B. */
#define CPU_LONG_BRANCH_MASK    0x0E0FU

/* The EF1 to EF4 pins are one bit each in the flag byte, EF1 the lowest. */
#define CPU_EF1                 0x01
#define CPU_EF2                 0x02
#define CPU_EF3                 0x04
#define CPU_EF4                 0x08

/* ------------------------------------------------------------------ */
/*  The Elf board                                                       */
/* ------------------------------------------------------------------ */

#define ELF_RAM_SIZE            16384    /* bytes at 0000, a power of two    */
#define ELF_RAM_MASK            (ELF_RAM_SIZE - 1)

/* What the data bus carries when nothing drives it: a read from an address
 * with no memory, or an INP from a port with no device. */
#define ELF_UNMAPPED_READ       0x00
#define ELF_FLOATING_BUS        0x00

/* I/O ports (the N field of an INP or OUT instruction). */
#define ELF_PORT_VIDEO          1        /* INP 1 display on, OUT 1 display off */
#define ELF_PORT_KEYPAD         4        /* INP 4 reads the keypad latch        */
#define ELF_PORT_DISPLAY        4        /* OUT 4 writes the two hex displays   */

/* Flag inputs. A pin reads 1 when high; a button or a chip that is "active"
 * pulls its pin low. Unused flag pins are left high. */
#define ELF_EF_VIDEO            CPU_EF1  /* low in the four lines before and at
                                            the end of the picture, so B1 is
                                            true then                       */
#define ELF_EF_KEYBOARD         CPU_EF3  /* low while the ASCII keyboard has a
                                            key waiting (the strobe), so B3
                                            is true then                     */
#define ELF_EF_IN_BUTTON        CPU_EF4  /* low while the IN button is down
                                            (it grounds the pin), so B4 is
                                            true then                        */
#define ELF_EF_UNUSED           CPU_EF2

/* ------------------------------------------------------------------ */
/*  The ASCII keyboard: it holds the code of the key last pressed, which */
/*  INP 7 reads (again and again, it stays); EF3 is low from the key    */
/*  press until INP 7 has taken it. The next waiting key does not come   */
/*  up until KBD_GAP_CYCLES machine cycles later (a debounce, which also */
/*  gives a program time to read the code a second time, as some do).    */
/* ------------------------------------------------------------------ */
#define ELF_PORT_ASCII          7
#define ELF_KBD_FIFO            16       /* keys remembered before one is read */
#define ELF_KBD_GAP_CYCLES      4500     /* about 20 ms of machine cycles    */

/* ------------------------------------------------------------------ */
/*  The VDU: a memory-mapped screen of the MC6847 kind, 1K of video RAM  */
/*  at E000 of which the first 512 bytes (32 columns, 16 rows) show.     */
/*  Each byte is one character cell:                                     */
/*    bit 6 clear: a text character, bits 0 to 5 its code (0 to 31 are   */
/*                 @ and A to Z and the symbols above Z, 32 to 63 are    */
/*                 space to the question mark), bit 7 set = inverse;     */
/*    bit 6 set:   2 x 2 block graphics, bits 0 to 3 the blocks (bit 3   */
/*                 top left, 2 top right, 1 bottom left, 0 bottom right), */
/*                 bits 4 and 5 the colour.                              */
/*  The CHIP-8 games for this screen draw 64 x 32 pixels with the blocks. */
/* ------------------------------------------------------------------ */
#define ELF_VDU_BASE            0xE000
#define ELF_VDU_SIZE            0x0400
#define VDU_COLS                32
#define VDU_ROWS                16
#define VDU_CELLS               (VDU_COLS * VDU_ROWS)
#define VDU_BIT_BLOCKS          0x40     /* the cell is block graphics       */
#define VDU_BIT_INVERSE         0x80     /* a text cell is inverse video     */
#define VDU_CODE_MASK           0x3F     /* text character code              */
#define VDU_BLOCK_MASK          0x0F     /* which of the 4 blocks are lit    */
#define VDU_COLOUR_SHIFT        4
#define VDU_COLOUR_MASK         0x03     /* 0 green, 1 yellow, 2 blue, 3 red */

/* ------------------------------------------------------------------ */
/*  Q as sound: the Elf's Q line drives a speaker, so every machine     */
/*  cycle's Q level is averaged into samples at QAUDIO_RATE (see        */
/*  qaudio.c). A DC blocker keeps a steady Q silent.                    */
/* ------------------------------------------------------------------ */
#define QAUDIO_RATE             22050    /* samples a second                 */
#define QAUDIO_FIFO             2048     /* samples kept for the front end   */
#define QAUDIO_FULL             32767    /* a sample of Q high all through   */
#define QAUDIO_DC_SHIFT         7        /* the DC blocker's time constant is
                                            2 to this power samples (about
                                            6 ms; steady Q fades out in it)  */
#define QAUDIO_VOLUME           190      /* out of 256, the swing of a tone   */

/* A single step runs at most this many machine cycles (an IDL waits for the
 * video chip's interrupt, which comes every 3,700 cycles or so). */
#define ELF_STEP_LIMIT          50000

/* Intel HEX files (loading a program) */
#define HEX_OK                  0
#define HEX_ERR_FORMAT          1        /* not a well-formed HEX file       */
#define HEX_ERR_CHECKSUM        2        /* a record's checksum is wrong     */

/* ------------------------------------------------------------------ */
/*  The 1861 (Pixie) video chip                                         */
/*                                                                      */
/*  It counts machine cycles: 14 to a line, 262 lines to a frame. While  */
/*  the display is on it interrupts the CPU two lines before the picture */
/*  and then asks for a burst of 8 DMA cycles on each of 128 lines; the  */
/*  CPU's R0 register points at the picture bytes. The EFX pin (EF1 on   */
/*  the Elf) goes low in the last four lines before the picture and in   */
/*  the last four lines of it.                                           */
/* ------------------------------------------------------------------ */
#define PIXIE_CYCLES_PER_LINE   14
#define PIXIE_LINES_PER_FRAME   262
#define PIXIE_BYTES_PER_LINE    8        /* 8 DMA bytes make 64 pixels       */
#define PIXIE_PIXELS_PER_LINE   (PIXIE_BYTES_PER_LINE * 8)
#define PIXIE_DISPLAY_LINES     128      /* lines that carry picture data    */

#define PIXIE_INT_FIRST_LINE    78       /* INT low from the start of this   */
#define PIXIE_INT_END_LINE      80       /* line to the start of this one    */
#define PIXIE_DISPLAY_FIRST     80       /* first line with DMA              */
#define PIXIE_DISPLAY_END       (PIXIE_DISPLAY_FIRST + PIXIE_DISPLAY_LINES)
#define PIXIE_DMA_FIRST_CYCLE   2        /* the DMA burst starts this far into
                                            the line (cycles 2 to 9)         */
#define PIXIE_EFX_TOP_FIRST     76       /* EFX low on lines 76 to 79        */
#define PIXIE_EFX_BOTTOM_FIRST  204      /* and on lines 204 to 207          */

#endif /* ELF_CONST_H */
