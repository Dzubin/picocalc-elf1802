/*
 * mc10_const.h - constants of the TRS-80 MC-10 (mc10.c): the clock, the memory
 * map, the keyboard matrix and the program shown when no ROM is fitted.
 *
 * Author: Thomas Dzubin
 */
#ifndef MC10_CONST_H
#define MC10_CONST_H

/* Clock: the colour burst crystal, divided by four for the processor. */
#define MC10_CRYSTAL_HZ     3579545u
#define MC10_CPU_HZ         894886u     /* cycles a second (the crystal / 4) */

/* Memory map. RAM from 0x4000 (4K as sold, 20K with the expansion), the video
 * chip reads it from its start; the one hardware port is at 0xBFFF; the ROM
 * is 8K at 0xE000 (the reset vector is its last two bytes). */
#define MC10_RAM_BASE       0x4000u
#define MC10_RAM_4K         0x1000u
#define MC10_RAM_20K        0x5000u
#define MC10_RAM_MAX        MC10_RAM_20K
#define MC10_IO_ADDRESS     0xBFFFu
#define MC10_IO_PAGE        0xBF00u
#define MC10_ROM_BASE       0xE000u
#define MC10_ROM_SIZE       0x2000u
#define MC10_OPEN_BUS       0xFFu

/* The hardware port at 0xBFFF. Written: bits 2 to 6 are the video chip's mode pins,
 * bit 7 the one-bit sound output. Read: the keyboard (the rows port 1 selects). */
#define MC10_IO_GM2         0x04u       /* also the chip's INT/EXT pin */
#define MC10_IO_GM1         0x08u
#define MC10_IO_GM0         0x10u
#define MC10_IO_AG          0x20u
#define MC10_IO_CSS         0x40u
#define MC10_IO_SOUND       0x80u

/* Port 2 bits. */
#define MC10_P2_CASSETTE_OUT    0x01u
#define MC10_P2_KEY_LINE        0x02u   /* read: control, break or shift is down */
#define MC10_P2_RS232_IN        0x04u
#define MC10_P2_CTS             0x08u
#define MC10_P2_CASSETTE_IN     0x10u
#define MC10_P2_IDLE            0xEBu   /* what the unused bits read as */

/* The keyboard is eight rows of up to seven keys. Port 1 bit r low selects row r;
 * a key that is down pulls its column's bit low. The rows that carry the three
 * keys that also reach port 2 bit 1: */
#define MC10_ROWS           8
#define MC10_KEYLINE_ROWS   0x85u       /* rows 0, 2 and 7 */
#define MC10_KEYLINE_BIT    0x40u       /* bit 6 of those rows */

/* The matrix as { character, row, bit, shift }. A shifted symbol is the key with
 * SHIFT held; CONTROL with A, S, W or Z is left, right, up and down. The special
 * keys use the codes MC10_KEY_*. */
#define MC10_KEY_ENTER      '\r'
#define MC10_KEY_BREAK      0x03
#define MC10_KEY_CONTROL    0x01
#define MC10_KEY_SHIFT      0x02
#define MC10_SHIFT_ROW      7           /* where SHIFT is in the matrix */
#define MC10_SHIFT_BIT      6
#define MC10_KEYS { \
    { '@', 0, 0, 0 }, { 'H', 0, 1, 0 }, { 'P', 0, 2, 0 }, { 'X', 0, 3, 0 }, { '0', 0, 4, 0 }, { '8', 0, 5, 0 }, \
    { MC10_KEY_CONTROL, 0, 6, 0 }, \
    { 'A', 1, 0, 0 }, { 'I', 1, 1, 0 }, { 'Q', 1, 2, 0 }, { 'Y', 1, 3, 0 }, { '1', 1, 4, 0 }, { '9', 1, 5, 0 }, \
    { 'B', 2, 0, 0 }, { 'J', 2, 1, 0 }, { 'R', 2, 2, 0 }, { 'Z', 2, 3, 0 }, { '2', 2, 4, 0 }, { ':', 2, 5, 0 }, \
    { MC10_KEY_BREAK, 2, 6, 0 }, \
    { 'C', 3, 0, 0 }, { 'K', 3, 1, 0 }, { 'S', 3, 2, 0 }, { '3', 3, 4, 0 }, { ';', 3, 5, 0 }, \
    { 'D', 4, 0, 0 }, { 'L', 4, 1, 0 }, { 'T', 4, 2, 0 }, { '4', 4, 4, 0 }, { ',', 4, 5, 0 }, \
    { 'E', 5, 0, 0 }, { 'M', 5, 1, 0 }, { 'U', 5, 2, 0 }, { '5', 5, 4, 0 }, { '-', 5, 5, 0 }, \
    { 'F', 6, 0, 0 }, { 'N', 6, 1, 0 }, { 'V', 6, 2, 0 }, { MC10_KEY_ENTER, 6, 3, 0 }, { '6', 6, 4, 0 }, \
    { '.', 6, 5, 0 }, \
    { 'G', 7, 0, 0 }, { 'O', 7, 1, 0 }, { 'W', 7, 2, 0 }, { ' ', 7, 3, 0 }, { '7', 7, 4, 0 }, { '/', 7, 5, 0 }, \
    { MC10_KEY_SHIFT, 7, 6, 0 }, \
    /* the symbols over the digits and punctuation, typed with SHIFT */ \
    { '!', 1, 4, 1 }, { '"', 2, 4, 1 }, { '#', 3, 4, 1 }, { '$', 4, 4, 1 }, { '%', 5, 4, 1 }, { '&', 6, 4, 1 }, \
    { '\'', 7, 4, 1 }, { '(', 0, 5, 1 }, { ')', 1, 5, 1 }, { '*', 2, 5, 1 }, { '+', 3, 5, 1 }, { '<', 4, 5, 1 }, \
    { '=', 5, 5, 1 }, { '>', 6, 5, 1 }, { '?', 7, 5, 1 } }

/* The program shown when no ROM covers the reset vector (assembled for 0xFF00: it
 * writes "NO MC-10 ROM FITTED" and "ESC R: FIT MC10.ROM AT E000" into the video
 * memory, one LDAA / STAA pair a character, then loops). It sits in the last page
 * of memory with the reset vector, and any ROM fitted there replaces it. */
#define MC10_FALLBACK_PAGE      0xFF00u
#define MC10_FALLBACK_VECTOR    0xFEu           /* offset in the page of the reset vector */
#define MC10_FALLBACK_SIZE      232
#define MC10_FALLBACK_CODE { \
    0x86, 0x0E, 0xB7, 0x40, 0xC6, 0x86, 0x0F, 0xB7, 0x40, 0xC7, 0x86, 0x20, \
    0xB7, 0x40, 0xC8, 0x86, 0x0D, 0xB7, 0x40, 0xC9, 0x86, 0x03, 0xB7, 0x40, \
    0xCA, 0x86, 0x2D, 0xB7, 0x40, 0xCB, 0x86, 0x31, 0xB7, 0x40, 0xCC, 0x86, \
    0x30, 0xB7, 0x40, 0xCD, 0x86, 0x20, 0xB7, 0x40, 0xCE, 0x86, 0x12, 0xB7, \
    0x40, 0xCF, 0x86, 0x0F, 0xB7, 0x40, 0xD0, 0x86, 0x0D, 0xB7, 0x40, 0xD1, \
    0x86, 0x20, 0xB7, 0x40, 0xD2, 0x86, 0x06, 0xB7, 0x40, 0xD3, 0x86, 0x09, \
    0xB7, 0x40, 0xD4, 0x86, 0x14, 0xB7, 0x40, 0xD5, 0x86, 0x14, 0xB7, 0x40, \
    0xD6, 0x86, 0x05, 0xB7, 0x40, 0xD7, 0x86, 0x04, 0xB7, 0x40, 0xD8, 0x86, \
    0x05, 0xB7, 0x41, 0x02, 0x86, 0x13, 0xB7, 0x41, 0x03, 0x86, 0x03, 0xB7, \
    0x41, 0x04, 0x86, 0x20, 0xB7, 0x41, 0x05, 0x86, 0x12, 0xB7, 0x41, 0x06, \
    0x86, 0x3A, 0xB7, 0x41, 0x07, 0x86, 0x20, 0xB7, 0x41, 0x08, 0x86, 0x06, \
    0xB7, 0x41, 0x09, 0x86, 0x09, 0xB7, 0x41, 0x0A, 0x86, 0x14, 0xB7, 0x41, \
    0x0B, 0x86, 0x20, 0xB7, 0x41, 0x0C, 0x86, 0x0D, 0xB7, 0x41, 0x0D, 0x86, \
    0x03, 0xB7, 0x41, 0x0E, 0x86, 0x31, 0xB7, 0x41, 0x0F, 0x86, 0x30, 0xB7, \
    0x41, 0x10, 0x86, 0x2E, 0xB7, 0x41, 0x11, 0x86, 0x12, 0xB7, 0x41, 0x12, \
    0x86, 0x0F, 0xB7, 0x41, 0x13, 0x86, 0x0D, 0xB7, 0x41, 0x14, 0x86, 0x20, \
    0xB7, 0x41, 0x15, 0x86, 0x01, 0xB7, 0x41, 0x16, 0x86, 0x14, 0xB7, 0x41, \
    0x17, 0x86, 0x20, 0xB7, 0x41, 0x18, 0x86, 0x05, 0xB7, 0x41, 0x19, 0x86, \
    0x30, 0xB7, 0x41, 0x1A, 0x86, 0x30, 0xB7, 0x41, 0x1B, 0x86, 0x30, 0xB7, \
    0x41, 0x1C, 0x20, 0xFE \
}

#endif /* MC10_CONST_H */
