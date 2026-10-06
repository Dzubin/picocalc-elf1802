/*
 * editor_const.h - constants of the text editor and assembler screens
 * (editor.c): the size of the text, where things go on the screen, and the
 * wording of its menu.
 *
 * Author: Thomas Dzubin
 */
#ifndef EDITOR_CONST_H
#define EDITOR_CONST_H

#include "asm_const.h"

#define EDITOR_MAX_TEXT     16384   /* characters of source (the assembler's
                                       image buffer holds a file as it is read) */
#define EDITOR_LINE_MAX     ASM_LINE_MAX    /* longest line (the assembler's) */
#define EDITOR_TAB_WIDTH    8       /* TAB goes to the next multiple of this  */
#define EDITOR_NOTE_MAX     40      /* the longest note reported (a row)      */
#define EDITOR_FORMAT_BUF   96      /* room to format a note before it is cut */

/* The editing screen: 30 rows of text, a status row and a message row. */
#define EDITOR_TEXT_ROWS    30
#define EDITOR_ROW_STATUS   30
#define EDITOR_ROW_MESSAGE  31
#define EDITOR_HINT         "ESC menu   F1 help"

/* What editor_open_source() did. */
#define EDITOR_OPEN_OK      0       /* read, assembled and loaded into the RAM */
#define EDITOR_OPEN_ERRORS  1       /* read, but it has errors (or is too big): show it */
#define EDITOR_OPEN_FAILED  2       /* could not be read: the note says why     */

/* The command menu (ui_menu): a letter only where it is not a key of the keypad
 * screen (0-9, A-F, R, L, M, I); the other rows are chosen with the arrow keys. */
#define EDITOR_MENU_TITLE   "EDITOR"
#define EDITOR_MENU_HINT    "UP/DOWN  ENTER: choose  ESC: back to text"
#define EDITOR_MENU_ASSEMBLE    0
#define EDITOR_MENU_GO          1
#define EDITOR_MENU_DEBUG       2
#define EDITOR_MENU_LISTING     3
#define EDITOR_MENU_ERROR       4
#define EDITOR_MENU_SAVE        5
#define EDITOR_MENU_OPEN        6
#define EDITOR_MENU_NEW         7
#define EDITOR_MENU_DISASSEMBLE 8
#define EDITOR_MENU_LEAVE       9
#define EDITOR_MENU_ITEMS_TEXT { \
    { "P  Put the program in the RAM", 'p' }, \
    { "G  Go: assemble and run it", 'g' }, \
    { "   Debug: assemble, then debugger", 0 }, \
    { "   Listing and labels", 0 }, \
    { "   Go to the next error", 0 }, \
    { "S  Save the source on the SD card", 's' }, \
    { "O  Open a source file", 'o' }, \
    { "N  New (clear the text)", 'n' }, \
    { "   Disassemble the RAM into text", 0 }, \
    { "X  Leave the editor", 'x' } }

/* What the Elf screen is to do when the editor is left after an assembly. */
#define EDITOR_THEN_NONE    0
#define EDITOR_THEN_RUN     1       /* start the program                      */
#define EDITOR_THEN_DEBUG   2       /* open the debugger on it                */

/* The listing and label screens. */
#define LISTING_ROWS        29      /* lines of the listing on a screen       */
#define LISTING_ROW_HINT    31
#define LISTING_TEXT_COL    14      /* where the source text starts on a line */
#define LISTING_SYMBOL_COLS 2       /* labels across the screen               */

#endif /* EDITOR_CONST_H */
