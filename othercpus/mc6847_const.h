/*
 * mc6847_const.h - constants of the Motorola MC6847 video display generator
 * (mc6847.c): the mode pins, the screen geometry and timing, the colours and
 * the internal character set.
 *
 * Author: Thomas Dzubin
 */
#ifndef MC6847_CONST_H
#define MC6847_CONST_H

/* The mode pins, as the machine sets them in the chip's `mode` field. The
 * INV and A/S pins are not here: the chip takes them from bits 6 and 7 of each
 * byte it fetches. */
#define M6847_AG        0x01    /* graphics (1) or alphanumerics/semigraphics (0) */
#define M6847_GM0       0x02    /* graphics mode, three bits                  */
#define M6847_GM1       0x04
#define M6847_GM2       0x08
#define M6847_CSS       0x10    /* colour set select                          */
#define M6847_INTEXT    0x20    /* external alphanumerics / semigraphics 6    */
#define M6847_MODE_MASK 0x3F
#define M6847_GM_MASK   (M6847_GM0 | M6847_GM1 | M6847_GM2)
#define M6847_GM_SHIFT  1       /* GM0 is bit 1: the mode number is the pins shifted down */

/* Bits of a byte of display memory in the alphanumeric modes. */
#define M6847_BYTE_AS   0x80    /* semigraphics                               */
#define M6847_BYTE_INV  0x40    /* inverse video                              */
#define M6847_BYTE_NONE 0xFF    /* what a chip with no memory to read sees   */
#define M6847_CHAR_MASK 0x3F    /* the character code (an index in the font)  */
#define M6847_STRIPE_MASK 0x7F  /* the bits of the stripe pattern             */

/* Semigraphics: the 4 has one nibble of a byte for the blocks and three colour
 * bits above it; the 6 has six bits for the blocks and two colour bits. A block
 * row is that many lines of the 12-line cell. */
#define M6847_SG4_MASK          0x0F
#define M6847_SG6_MASK          0x3F
#define M6847_SG4_COLOR_SHIFT   4
#define M6847_SG4_COLOR_MASK    0x07
#define M6847_SG6_COLOR_SHIFT   6
#define M6847_SG6_COLOR_MASK    0x03
#define M6847_SG4_SLICE_LINES   6
#define M6847_SG6_SLICE_LINES   4

/* Eight pixels of a pattern: the leftmost is bit 7; a block is half of it. */
#define M6847_PATTERN_LEFTMOST  0x80
#define M6847_PATTERN_LEFT      0xF0
#define M6847_PATTERN_RIGHT     0x0F

/* Screen: 262 lines of 228 clocks of the 3.58 MHz colour clock, which is 57
 * cycles of a processor that runs at a quarter of it. 192 lines of 256 pixels
 * show the display; 32 columns of 8 pixels. */
#define M6847_LINES             262
#define M6847_CLOCKS_PER_LINE   228
#define M6847_CYCLES_PER_LINE   57
#define M6847_WIDTH             256
#define M6847_HEIGHT            192
#define M6847_COLUMNS           32
#define M6847_TOP_BORDER        25      /* lines before the display           */
#define M6847_FS_LOW_FIRST      216     /* field sync is low from this line ... */
#define M6847_FS_LOW_LAST       247     /* ... to this one                    */
#define M6847_CELL_HEIGHT       12      /* lines of a character cell          */

/* Palette indices. 0 to 7 are the colours (green, yellow, blue, red, buff, cyan,
 * magenta, orange), 8 is black, 9 to 11 the graphics foregrounds, 12 to 15 the
 * text colours (dark and bright green, dark and bright orange). */
#define M6847_BLACK             8
#define M6847_PALETTE_SIZE      16

/* Where each colour set starts. Four-colour graphics: green, yellow, blue, red
 * (CSS 0) or buff, cyan, magenta, orange (CSS 1), which is also the border of a
 * graphics mode (the first colour of the set). Two-colour graphics: black and
 * green, or black and buff. Text: the background colour, and the foreground is
 * the next one. */
#define M6847_PAL_CG_SET0       0
#define M6847_PAL_CG_SET1       4
#define M6847_PAL_RG_SET0       8
#define M6847_PAL_RG_SET1       10
#define M6847_PAL_TEXT_SET0     12
#define M6847_PAL_TEXT_SET1     14

/* Colours as 0xRRGGBB, from the figures MAME uses (measured from the chip). */
#define M6847_PALETTE { \
    0x30D200, 0xC1E500, 0x4C3AB4, 0x9A3236, 0xBFC8AD, 0x41AF71, 0xC84EF0, 0xD47F00, \
    0x263016, 0x30D200, 0x263016, 0xBFC8AD, 0x007C00, 0x30D200, 0x6B2700, 0xFFB700 }

/* The built-in character set: 64 characters (the codes 0x00 to 0x3F are @ and A
 * to Z, then [ backslash ] up-arrow left-arrow, then space to ?), 5 pixels
 * wide and 7 high, the leftmost pixel in bit 4. A character is drawn in an 8 x
 * 12 cell with its first row on the cell's fourth line and its first column on
 * the cell's third pixel. */
#define M6847_FONT_ROWS         7
#define M6847_FONT_FIRST_LINE   3
#define M6847_FONT_FIRST_COLUMN 2
#define M6847_FONT_CHARS        64
#define M6847_FONT_WIDTH        5
#define M6847_FONT_SHIFT        (7 - M6847_FONT_FIRST_COLUMN - (M6847_FONT_WIDTH - 1))
#define M6847_FONT { \
    { 0x0E, 0x11, 0x01, 0x0D, 0x15, 0x15, 0x0E },   /* 00 @ */ \
    { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 },   /* 01 A */ \
    { 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E },   /* 02 B */ \
    { 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E },   /* 03 C */ \
    { 0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E },   /* 04 D */ \
    { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F },   /* 05 E */ \
    { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10 },   /* 06 F */ \
    { 0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F },   /* 07 G */ \
    { 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 },   /* 08 H */ \
    { 0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E },   /* 09 I */ \
    { 0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C },   /* 0A J */ \
    { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 },   /* 0B K */ \
    { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F },   /* 0C L */ \
    { 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11 },   /* 0D M */ \
    { 0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11 },   /* 0E N */ \
    { 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E },   /* 0F O */ \
    { 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10 },   /* 10 P */ \
    { 0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D },   /* 11 Q */ \
    { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 },   /* 12 R */ \
    { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E },   /* 13 S */ \
    { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 },   /* 14 T */ \
    { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E },   /* 15 U */ \
    { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04 },   /* 16 V */ \
    { 0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11 },   /* 17 W */ \
    { 0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11 },   /* 18 X */ \
    { 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04 },   /* 19 Y */ \
    { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F },   /* 1A Z */ \
    { 0x0E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0E },   /* 1B [ */ \
    { 0x00, 0x10, 0x08, 0x04, 0x02, 0x01, 0x00 },   /* 1C backslash */ \
    { 0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E },   /* 1D ] */ \
    { 0x04, 0x0E, 0x15, 0x04, 0x04, 0x04, 0x04 },   /* 1E ^ */ \
    { 0x00, 0x04, 0x08, 0x1F, 0x08, 0x04, 0x00 },   /* 1F _ */ \
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   /* 20   */ \
    { 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04 },   /* 21 ! */ \
    { 0x0A, 0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00 },   /* 22 " */ \
    { 0x0A, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x0A },   /* 23 # */ \
    { 0x04, 0x0F, 0x14, 0x0E, 0x05, 0x1E, 0x04 },   /* 24 $ */ \
    { 0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03 },   /* 25 % */ \
    { 0x0C, 0x12, 0x14, 0x08, 0x15, 0x12, 0x0D },   /* 26 & */ \
    { 0x04, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00 },   /* 27 ' */ \
    { 0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02 },   /* 28 ( */ \
    { 0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08 },   /* 29 ) */ \
    { 0x00, 0x04, 0x15, 0x0E, 0x15, 0x04, 0x00 },   /* 2A star */ \
    { 0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00 },   /* 2B + */ \
    { 0x00, 0x00, 0x00, 0x00, 0x0C, 0x04, 0x08 },   /* 2C , */ \
    { 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00 },   /* 2D - */ \
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C },   /* 2E . */ \
    { 0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x00 },   /* 2F slash */ \
    { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E },   /* 30 0 */ \
    { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E },   /* 31 1 */ \
    { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F },   /* 32 2 */ \
    { 0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E },   /* 33 3 */ \
    { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 },   /* 34 4 */ \
    { 0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E },   /* 35 5 */ \
    { 0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E },   /* 36 6 */ \
    { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 },   /* 37 7 */ \
    { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E },   /* 38 8 */ \
    { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C },   /* 39 9 */ \
    { 0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00 },   /* 3A : */ \
    { 0x0C, 0x0C, 0x00, 0x0C, 0x04, 0x08, 0x00 },   /* 3B ; */ \
    { 0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02 },   /* 3C < */ \
    { 0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00 },   /* 3D = */ \
    { 0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08 },   /* 3E > */ \
    { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04 },   /* 3F ? */ \
}

/* Bytes in a line and display rows, by the graphics mode (GM2 GM1 GM0): the chip
 * repeats each row for enough lines to fill the 192, and the machine's memory
 * is read at row * bytes + column. Pixels per byte: 4 (two bits each) for the
 * colour modes, 8 for the two-colour modes; "wide" is the width in screen
 * pixels of one picture pixel. */
#define M6847_GRAPHICS_MODES { \
    /* bytes rows bits wide */ \
    { 16,  64, 2, 4 },   /* 0: CG1 64 x 64, four colours            */ \
    { 16,  64, 1, 2 },   /* 1: RG1 128 x 64, two colours            */ \
    { 32,  64, 2, 2 },   /* 2: CG2 128 x 64, four colours           */ \
    { 16,  96, 1, 2 },   /* 3: RG2 128 x 96, two colours            */ \
    { 32,  96, 2, 2 },   /* 4: CG3 128 x 96, four colours           */ \
    { 16, 192, 1, 2 },   /* 5: RG3 128 x 192, two colours           */ \
    { 32, 192, 2, 2 },   /* 6: CG6 128 x 192, four colours          */ \
    { 32, 192, 1, 1 } }  /* 7: RG6 256 x 192, two colours           */

#endif /* MC6847_CONST_H */
