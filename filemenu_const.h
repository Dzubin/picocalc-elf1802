/*
 * filemenu_const.h - constants of the menu and of saving and opening programs.
 *
 * Author: Thomas Dzubin
 */
#ifndef FILEMENU_CONST_H
#define FILEMENU_CONST_H

#define FILEMENU_NOTE_MAX   40      /* the longest note reported (a row of the screen) */

#define CONFIRM_ROW         14      /* the question; its answers are 2 and 3 rows down */
#define MENU_LABEL_MAX      40      /* characters of a row of the menu         */
#define MENU_MAX_ROWS       24      /* headings and items                      */
#define MENU_TITLE          "MENU"
#define MENU_HINT           "UP/DOWN  ENTER: choose  ESC: back"
#define MENU_ROM_HINT       "UP/DOWN  ENTER: choose a slot  ESC: back"

#define LIST_ROW_TITLE       2
#define LIST_ROW_FIRST       5      /* file names, one a row                 */
#define LIST_ROWS_SHOWN     18
#define LIST_ROW_HINT       28
#define LIST_COL_NAME        4
#define LIST_MAX_FILES      64      /* more than this are not shown          */

#define SAVE_ROW_TITLE       2
#define SAVE_ROW_PROMPT      8
#define SAVE_ROW_NAME       10
#define SAVE_ROW_RESULT     14
#define SAVE_ROW_HINT       28
#define SAVE_NAME_MAX        8      /* letters and digits before the .BIN    */

#define ROMUI_ROW_TITLE      2
#define ROMUI_ROW_FIRST      6      /* the slots, 2 rows apart               */
#define ROMUI_ROW_NOTE      20
#define ROMUI_ROW_HINT      28
#define ROMUI_ADDRESS_DIGITS 4      /* hex digits of a ROM's address         */

#define LISTING_CHUNK     4096      /* bytes of a listing written at a time     */

#define HEX_FILE_MAX     49152      /* longest .HEX file read: a full RAM in
                                       16-byte records is about 46,000 bytes */

#endif /* FILEMENU_CONST_H */
