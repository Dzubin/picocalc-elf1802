/*
 * mcardui_const.h - constants of the Membership Card's screen (mcardui.c): the
 * rows of its front panel under the terminal, the keys and the texts.
 *
 * Author: Thomas Dzubin
 */
#ifndef MCARDUI_CONST_H
#define MCARDUI_CONST_H

/* The terminal fills the top 16 text rows; the front panel is under it. */
#define MCUI_ROW_TITLE      16
#define MCUI_ROW_LED_HEAD   18
#define MCUI_ROW_LED        19
#define MCUI_ROW_SW_HEAD    21
#define MCUI_ROW_SW         22
#define MCUI_ROW_STATUS     24
#define MCUI_ROW_SWITCHES   25
#define MCUI_ROW_IN         26
#define MCUI_ROW_HELP_1     28
#define MCUI_ROW_HELP_2     29
#define MCUI_ROW_MESSAGE    30

#define MCUI_TITLE          "1802 MEMBERSHIP CARD  Rev L  4 MHz"
#define MCUI_HEAD           "7 6 5 4 3 2 1 0"
#define MCUI_COL_BITS       12              /* the column the eight bits start at  */
#define MCUI_COL_HEX        31              /* the hex display                      */

/* The keys of the panel (the digits 0 to 7 flip the data switches). */
#define MCUI_KEY_WAIT       'w'
#define MCUI_KEY_CLR        'c'
#define MCUI_KEY_WRITE      'r'
#define MCUI_KEY_IN         'i'             /* ENTER does it too                    */

#define MCUI_REFRESH_MS     50              /* the panel rows are checked this often */
#define MCUI_TERM_MS        20              /* the terminal is redrawn this often    */
#define MCUI_MENU_TITLE     "1802 MEMBERSHIP CARD"
#define MCUI_MENU_HINT      "UP DOWN ENTER, ESC: BACK"
#define MCUI_MENU_LABEL_MAX 40

#endif /* MCARDUI_CONST_H */
