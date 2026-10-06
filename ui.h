/*
 * ui.h - the screen and keyboard helpers that every screen of Elf1802 shares:
 * text on the 40 x 32 character grid (kept to the width of a row, which the
 * vendored lcd_putstr() does not do for us), rows that are redrawn only when
 * their text changes, the keyboard events from the south bridge, and the two
 * ways out of the program (BOOTSEL and the UF2 Loader).
 *
 * The keyboard is read straight from the south bridge's FIFO: each call to
 * ui_key_event() returns one 16-bit event, (state << 8) | code, or 0 when the
 * FIFO is empty. Nothing else drains that FIFO, because picocalc_init() is not
 * called.
 *
 * Author: Thomas Dzubin
 */
#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include <stdint.h>

/* Text. All of these cut the string at the right edge of the screen. */
void ui_clear_row(int row);
void ui_put_row(int row, const char *text, uint16_t fg);            /* centred */
void ui_put_at(int col, int row, const char *text, uint16_t fg);

/* A row of text that is drawn only when it differs from what the row shows now
 * (so a screen can be "redrawn" every turn at no cost). ui_forget_rows() makes
 * the rows from first_row down look unknown, after something else has drawn
 * over them. */
void ui_panel_row(int row, int col, const char *text, uint16_t fg);
void ui_forget_rows(int first_row);
void ui_forget_row_span(int first_row, int last_row);

/* The keyboard. */
uint16_t ui_key_event(void);                    /* 0 when nothing is waiting */
uint8_t  ui_ev_state(uint16_t event);           /* KEY_STATE_*               */
uint8_t  ui_ev_code(uint16_t event);
bool     ui_is_mod_key(uint8_t code);           /* a bare SHIFT, CTRL, ALT   */
uint8_t  ui_lower(uint8_t code);
void     ui_drain_keys(void);

/* Wait for a real key press and return its code. The ~ key (SHIFT+backtick)
 * goes to BOOTSEL from here, so it works on every screen that waits. */
uint8_t ui_wait_any_key(void);

/* The next key press, waiting for it. Unlike ui_wait_any_key() it does not throw
 * away the keys already waiting, so fast typing (an editor) loses none; the ~
 * key still goes to BOOTSEL. */
uint8_t ui_read_key(void);

/* A menu that is chosen from with the arrow keys. Every screen's menu uses it, so
 * they all work the same way: UP and DOWN move the arrow (headings are passed
 * over), ENTER chooses, ESC leaves, and an item that has a shortcut letter is
 * chosen by pressing it. Shortcut letters must not be keys that the hex keypad
 * screen uses (0-9, A-F, R, L, M, I). */
typedef struct {
    const char *text;       /* the row; "#NAME" is a heading, not an item         */
    char        key;        /* the shortcut letter in lower case, or 0 for none   */
} ui_menu_item_t;

/* Draw the menu (title at the top, hint at the bottom, count rows of items) and
 * wait for a choice. start is the item the arrow starts on. Returns the index
 * of the item chosen, or -1 if the user pressed ESC. The caller redraws its own
 * screen afterwards. */
int ui_menu(const char *title, const ui_menu_item_t *items, int count, const char *hint,
            int start);

/* Leave the program; neither returns. */
_Noreturn void ui_go_bootsel(void);             /* the USB drive (BOOTSEL) mode */
_Noreturn void ui_go_loader(void);              /* back to the UF2 Loader menu  */

#endif /* UI_H */
