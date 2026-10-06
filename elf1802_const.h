/*
 * elf1802_const.h - the PicoCalc side constants of Elf1802: version, colours,
 * screen positions, key assignments, timing and the seven-segment tables.
 * elf1802.c holds only code, so a new constant or a moved row goes here. The
 * constants of the emulated computer itself are in elf_const.h.
 *
 * Author: Thomas Dzubin
 */
#ifndef ELF1802_CONST_H
#define ELF1802_CONST_H

#include <stdint.h>

#include "elf_const.h"

/* Shown on the splash screen; the firmware build reads it from this line, so
 * keep it in the form  #define VERSION "V0.10A"  on one line. */
#define VERSION "V0.10A"

/* ------------------------------------------------------------------ */
/*  Colours (full-strength only: dim greys are hard to read on the LCD) */
/* ------------------------------------------------------------------ */
#define COL_BG      RGB(  0,   0,   0)
#define COL_WHITE   RGB(235, 235, 235)
#define COL_RED     RGB(255,  60,  60)
#define COL_GREEN   RGB( 90, 235, 130)
#define COL_YELLOW  RGB(255, 225,  70)
#define COL_CYAN    RGB(120, 225, 255)
#define COL_LED     RGB(255,  40,  40)       /* the hex displays and Q LED   */

/* ------------------------------------------------------------------ */
/*  Screen geometry: the LCD is a 40 x 32 grid of 8 x 10 pixel cells    */
/* ------------------------------------------------------------------ */
#define TCOLS    40

/* ------------------------------------------------------------------ */
/*  The top of the screen: the 1861's picture, the VDU, or the register  */
/*  read-out (picture.c, vdu.c and regview.c draw them in this area)     */
/* ------------------------------------------------------------------ */
#define TOP_AREA_HEIGHT    160      /* the picture, the VDU or the register read-out fills this */

/* ------------------------------------------------------------------ */
/*  The front panel, below the picture                                  */
/* ------------------------------------------------------------------ */
#define ROW_MODE            16      /* the title line under the picture      */

/* The two hex displays and the Q LED, in pixels. */
#define DISPLAY_Y           170
#define DISPLAY_X_HIGH      16      /* left digit                            */
#define DISPLAY_X_LOW       52      /* right digit                           */
#define DIGIT_W             24
#define DIGIT_H             44
#define DISPLAY_LABEL_ROW   22
#define DISPLAY_LABEL_COL   3
#define Q_LED_X             104
#define Q_LED_Y             176
#define Q_LED_SIZE          24
#define Q_LED_BORDER        2
#define Q_LABEL_ROW         20
#define Q_LABEL_COL         13

/* Text beside them (columns from the left). */
#define PANEL_COL           20
#define ROW_SW_RUN          17
#define ROW_SW_LOAD         18
#define ROW_SW_MP           19
#define ROW_IN_KEYPAD       20
#define ROW_PANEL_MODE      21

/* The CPU and speed read-out. */
#define ROW_REG_1           23
#define ROW_REG_2           24
#define ROW_REG_3           25
#define ROW_MESSAGE         26      /* a short note: saved, loaded, errors   */

/* Help lines at the bottom. */
#define ROW_HELP_1          27
#define ROW_HELP_2          28
#define ROW_HELP_3          29
#define ROW_HELP_4          30

/* ------------------------------------------------------------------ */
/*  The Elf II front panel (the default screen), drawn under the       */
/*  picture in the 320 x 160 pixels below TOP_AREA_HEIGHT: the title    */
/*  and the two hex displays and the Q LED on the left, the 4 x 4 keypad */
/*  in the middle, the RUN, LOAD and M/P toggles and the IN button on    */
/*  the right. Elements are drawn into a small canvas and blitted.       */
/* ------------------------------------------------------------------ */
#define PANEL_CANVAS_PIXELS 3200    /* room for the biggest element (the 80 x 38 tag) */

/* The left column is 100 pixels wide, so its middle is at x = 50: the title,
 * the two displays (24 wide with a gap of 8) and the Q LED (20 wide) are all
 * centred on it, one under the other. */
#define PANEL_TITLE_X       9
#define PANEL_TITLE_Y       172
#define PANEL_TITLE_SCALE   2
#define PANEL_TITLE_TEXT    "ELF1802"

#define PANEL_DISPLAY_Y     196
#define PANEL_DISPLAY_X_HIGH 22
#define PANEL_DISPLAY_X_LOW  54
#define PANEL_Q_X           40
#define PANEL_Q_Y           250
#define PANEL_Q_SIZE        20
#define PANEL_Q_BORDER      2

#define PANEL_KEY_X         100     /* top left of the C key                  */
#define PANEL_KEY_Y         168
#define PANEL_KEY_PITCH_X   42
#define PANEL_KEY_PITCH_Y   36
#define PANEL_KEY_W         38
#define PANEL_KEY_H         32
#define PANEL_KEY_EDGE      3       /* the lower edge that makes it look raised */
#define PANEL_KEY_LABEL_X   11
#define PANEL_KEY_LABEL_Y   5
#define PANEL_KEY_LABEL_SCALE 3
#define PANEL_KEY_BORDER    2       /* the white frame of a pressed key       */
/* The hex digit on each key, row by row from the top left. */
#define PANEL_KEY_VALUES { 12, 13, 14, 15,  8, 9, 10, 11,  4, 5, 6, 7,  0, 1, 2, 3 }

#define PANEL_SW_X          282     /* centre of the toggles                  */
#define PANEL_SW_RUN_Y      182     /* centre of each pivot                   */
#define PANEL_SW_LOAD_Y     220
#define PANEL_SW_MP_Y       258
#define PANEL_SW_CANVAS_W   16
#define PANEL_SW_CANVAS_H   40      /* the lever and its ball either way up   */
#define PANEL_SW_LABEL_X    294
#define PANEL_SW_LABEL_DY   (-3)    /* label top, from the pivot's centre     */
#define PANEL_SW_LABEL_W    24      /* the longest label, four letters        */
#define PANEL_SW_LABEL_H    9       /* a letter and the line under the first  */
#define PANEL_SW_UNDERLINE_Y 8      /* the first letter is the key to press   */

/* The tag under the Q LED (the real panel had nothing there). While the keys
 * go to the ASCII keyboard it is a yellow "ASCII" pill. While a program is
 * waiting for that keyboard (it tests flag EF3 or reads port 7) and the keys
 * are still going to the hex keypad, it is a pill that pulses between two
 * yellows and reads KBD? over F4 (the key that switches). The ASCII pill says
 * how to get out of that mode, F4=OFF. */
#define PANEL_TAG_X         10
#define PANEL_TAG_Y         286
#define PANEL_TAG_W         80
#define PANEL_TAG_H         34
#define PANEL_ASCII_TEXT    "ASCII"
#define PANEL_ASCII_EXIT    "F4=OFF"
#define PANEL_HINT_LINE_1   "KBD?"
#define PANEL_HINT_LINE_2   "F4"

/* "MENU:ESC" under the Q LED, centred on it, with the key in yellow. */
#define PANEL_MENU_LABEL    "MENU:"
#define PANEL_MENU_KEY      "ESC"
#define PANEL_MENU_Y        278
#define PANEL_MENU_X        27
#define KBD_HINT_HOLD_MS    1500    /* the hint stays this long after the last
                                       test of EF3 or read of port 7         */
#define KBD_HINT_PERIOD_MS  1600    /* one slow pulse                         */
#define KBD_HINT_LEVELS     8       /* steps from the dull yellow to the bright */
#define KBD_HINT_LOW_R      190     /* the dull yellow of the pulse           */
#define KBD_HINT_LOW_G      160
#define KBD_HINT_LOW_B      40
#define KBD_HINT_HIGH_R     255     /* the bright one (the same as COL_YELLOW) */
#define KBD_HINT_HIGH_G     225
#define KBD_HINT_HIGH_B     70

#define PANEL_IN_X          272
#define PANEL_IN_Y          284
#define PANEL_IN_W          40
#define PANEL_IN_H          26

#define COL_KEYCAP          COL_WHITE
#define COL_KEYCAP_EDGE     RGB(200, 200, 200)
#define COL_KEY_LABEL       RGB(  0,   0,   0)

/* 5 x 7 pixel letters and digits for the panel, one row of five bits a byte
 * (the leftmost pixel is bit 4). */
typedef struct {
    char    ch;
    uint8_t rows[7];
} panel_glyph_t;

#define PANEL_GLYPH_W       5
#define PANEL_GLYPH_H       7
#define PANEL_GLYPH_ADVANCE 6       /* pixels from one letter to the next     */

#define PANEL_GLYPHS { \
    { '0', { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E } }, \
    { '1', { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E } }, \
    { '2', { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F } }, \
    { '3', { 0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E } }, \
    { '4', { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 } }, \
    { '5', { 0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E } }, \
    { '6', { 0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E } }, \
    { '7', { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 } }, \
    { '8', { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E } }, \
    { '9', { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C } }, \
    { 'A', { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 } }, \
    { 'B', { 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E } }, \
    { 'C', { 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E } }, \
    { 'D', { 0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E } }, \
    { 'E', { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F } }, \
    { 'F', { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10 } }, \
    { 'I', { 0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E } }, \
    { 'K', { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 } }, \
    { '?', { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04 } }, \
    { 'L', { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F } }, \
    { 'M', { 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11 } }, \
    { 'N', { 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11 } }, \
    { 'O', { 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E } }, \
    { 'P', { 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10 } }, \
    { 'R', { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 } }, \
    { 'S', { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E } }, \
    { 'U', { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E } }, \
    { '/', { 0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10 } }, \
    { ':', { 0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00 } }, \
    { ' ', { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } } }

/* ------------------------------------------------------------------ */
/*  Seven-segment digits drawn from rectangles                          */
/* ------------------------------------------------------------------ */
#define SEG_COUNT           7
#define SEG_X               0       /* fields of a segment rectangle         */
#define SEG_Y               1
#define SEG_W               2
#define SEG_H               3

/* The segments a b c d e f g as x, y, width, height inside a DIGIT_W by
 * DIGIT_H cell (a is the top bar, then clockwise, g the middle). */
#define SEGMENT_RECTS { \
    {  4,  0, 16,  4 }, \
    { 20,  4,  4, 16 }, \
    { 20, 24,  4, 16 }, \
    {  4, 40, 16,  4 }, \
    {  0, 24,  4, 16 }, \
    {  0,  4,  4, 16 }, \
    {  4, 20, 16,  4 } }

/* Which segments are lit for 0 to F: bit 0 is a, bit 6 is g. */
#define SEGMENT_DIGITS { \
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, \
    0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71 }

/* ------------------------------------------------------------------ */
/*  Keys                                                                */
/* ------------------------------------------------------------------ */
#define KEY_IN_BUTTON       'i'     /* the IN button (ENTER does it too)     */
#define KEY_SW_RUN          'r'
#define KEY_SW_LOAD         'l'
#define KEY_SW_MP           'm'
#define KEY_VIEW            'v'     /* show the VDU or the picture (hex mode) */
#define KEY_SCREEN_TOGGLE   KEY_TAB /* the panel screen <-> the details screen */
#define KEY_BOOT_PROGRAM    'b'     /* on the title screen: reload the boot program */
#define KEY_HELP            'h'     /* the help screen (H, h or ?)            */
#define KEY_HELP_QUESTION   '?'

/* Function keys, in either keyboard mode. Only the unshifted F1 to F5 are used
 * (F6 to F10 are Shift+F1 to F5 on the PicoCalc). Everything else is reached
 * from the ESC menu; RUN, LOAD, IN and memory protect are the letters R, L, I
 * and M, and on the ASCII keyboard (where every letter is typed to the program)
 * memory protect is in the menu and RUN, LOAD and IN are not available. */
#define FKEY_HELP           KEY_F1  /* the help screen (H and ? do it too, off the ASCII keyboard) */
#define FKEY_ASCII          KEY_F4  /* hex keypad <-> ASCII keyboard: the one key that does both */
#define FKEY_ESC            KEY_F5  /* sends ESC (1B) to the program         */

/* What the ASCII keyboard sends. */
#define ASCII_ENTER         0x0D    /* the PicoCalc's ENTER sends CR         */
#define ASCII_BACKSPACE     0x08
#define ASCII_ESCAPE        0x1B
#define ASCII_UPPERCASE_ONLY 1      /* letters go out in capitals (BASIC)    */

/* ------------------------------------------------------------------ */
/*  Timing and sound                                                    */
/* ------------------------------------------------------------------ */
#define POLL_MS             1       /* pause between turns of the main loop  */
#define MAX_CATCHUP_US      100000  /* never try to make up more than this   */
#define KEY_POLL_MS         33      /* the keyboard is read this often: every
                                       read is a 4 to 5 ms wait on a 10 kHz
                                       bus, and the chip needs a read only
                                       every 2.5 s to stay awake             */
#define KEY_STALE_MS        500     /* a key down this long with no repeat
                                       event is treated as let go (its release
                                       can arrive under another code)        */
#define PANEL_REFRESH_MS    100     /* register read-out update interval     */
#define MESSAGE_MS          3000    /* how long a note stays on the screen   */
#define SPEED_WINDOW_MS     1000    /* the speed percentage is measured over */

/* The Q line as sound: the samples the board makes (qaudio.c) are collected
 * this many at a time on every turn of the main loop and handed to the
 * platform. The menu turns the sound off and on. */
#define SOUND_CHUNK         256
#define SOUND_ON_AT_START   1
#define SOUND_IDLE_MS       50      /* no samples for this long: silence      */

#endif /* ELF1802_CONST_H */
