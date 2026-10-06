/*
 * ui.c - the screen and keyboard helpers shared by every screen. See ui.h.
 *
 * Author: Thomas Dzubin
 */
#include <string.h>

#include "lcd.h"          /* WIDTH, GLYPH_HEIGHT, ROWS, lcd_*()               */
#include "southbridge.h"  /* sb_read_keyboard()                               */
#include "keyboard.h"     /* KEY_* codes, KEY_STATE_* values                  */

#include "platform.h"
#include "elf1802_const.h"
#include "ui.h"
#include "ui_const.h"

static char shown_text[ROWS][TCOLS + 1];        /* what each row shows now   */

void ui_clear_row(int row)
{
    lcd_solid_rectangle(COL_BG, 0, (uint16_t)(row * GLYPH_HEIGHT), WIDTH,
                        GLYPH_HEIGHT);
}

/* print a string at a column and row, cut off at the right edge (the driver's
 * lcd_putstr() has to be given nothing longer than the row) */
void ui_put_at(int col, int row, const char *str, uint16_t fg)
{
    char text[TCOLS + 1];
    int room = TCOLS - col;

    if (room <= 0 || !str[0])
        return;
    strncpy(text, str, (size_t)room);
    text[room] = '\0';
    lcd_set_foreground(fg);
    lcd_putstr((uint8_t)col, (uint8_t)row, text);
}

/* clear a row and print a string centred on it */
void ui_put_row(int row, const char *str, uint16_t fg)
{
    int len = (int)strlen(str);
    int col = (TCOLS - len) / 2;

    ui_clear_row(row);
    if (col < 0)
        col = 0;
    ui_put_at(col, row, str, fg);
}

void ui_panel_row(int row, int col, const char *text, uint16_t fg)
{
    size_t n;

    if (strcmp(shown_text[row], text) == 0)
        return;
    n = strlen(text);
    if (n > TCOLS)
        n = TCOLS;
    memcpy(shown_text[row], text, n);
    shown_text[row][n] = '\0';
    lcd_erase_line((uint8_t)row, (uint8_t)col, TCOLS - 1);
    ui_put_at(col, row, text, fg);
}

/* A forgotten row holds a mark that no row's text can be, so the next
 * ui_panel_row() draws it even if the new text is empty (an empty row may have
 * a cursor or some other drawing on it to wipe). */
static void forget_row(int row)
{
    shown_text[row][0] = UI_FORGOTTEN_MARK;
    shown_text[row][1] = '\0';
}

void ui_forget_rows(int first_row)
{
    int row;

    for (row = first_row; row < ROWS; row++)
        forget_row(row);
}

void ui_forget_row_span(int first_row, int last_row)
{
    int row;

    for (row = first_row; row <= last_row && row < ROWS; row++)
        forget_row(row);
}

/* ------------------------------------------------------------------ */
/*  Keyboard (raw south-bridge FIFO polling)                            */
/* ------------------------------------------------------------------ */
uint16_t ui_key_event(void)           { return sb_read_keyboard(); }
uint8_t  ui_ev_state(uint16_t event)  { return (uint8_t)(event >> 8); }
uint8_t  ui_ev_code(uint16_t event)   { return (uint8_t)(event & 0xFF); }

bool ui_is_mod_key(uint8_t code)
{
    return code == KEY_MOD_SHL || code == KEY_MOD_SHR || code == KEY_MOD_CTRL ||
           code == KEY_MOD_ALT || code == KEY_MOD_SYM;
}

uint8_t ui_lower(uint8_t code)
{
    return (code >= 'A' && code <= 'Z') ? (uint8_t)(code + 'a' - 'A') : code;
}

void ui_drain_keys(void)
{
    while (ui_key_event() != 0)
        ;                     /* reading the FIFO empties it */
}

uint8_t ui_wait_any_key(void)
{
    ui_drain_keys();
    for (;;) {
        uint16_t e = ui_key_event();

        if (e && ui_ev_state(e) == KEY_STATE_PRESSED) {
            uint8_t code = ui_ev_code(e);

            if (code == '~')
                ui_go_bootsel();
            if (!ui_is_mod_key(code)) {
                ui_drain_keys();
                return code;
            }
        }
        plat_sleep_ms(UI_POLL_MS);
    }
}

uint8_t ui_read_key(void)
{
    for (;;) {
        uint16_t e = ui_key_event();

        if (e && ui_ev_state(e) == KEY_STATE_PRESSED) {
            uint8_t code = ui_ev_code(e);

            if (code == '~')
                ui_go_bootsel();
            if (!ui_is_mod_key(code))
                return code;
        }
        if (!e)
            plat_sleep_ms(UI_POLL_MS);
    }
}

/* ------------------------------------------------------------------ */
/*  The menu                                                            */
/* ------------------------------------------------------------------ */
static bool is_heading(const ui_menu_item_t *item)
{
    return item->text[0] == UI_MENU_HEADING;
}

/* Draw one row of the menu; the chosen item gets the arrow and the yellow. */
static void menu_row(const ui_menu_item_t *item, int index, bool chosen)
{
    int row = UI_MENU_ROW_FIRST + index;

    ui_clear_row(row);
    if (is_heading(item)) {
        ui_put_at(UI_MENU_COL_HEADING, row, item->text + 1, COL_CYAN);
        return;
    }
    if (chosen)
        ui_put_at(UI_MENU_COL_MARK, row, UI_MENU_MARK, COL_YELLOW);
    ui_put_at(UI_MENU_COL_TEXT, row, item->text, chosen ? COL_YELLOW : COL_WHITE);
}

/* The next item that is not a heading, going down (step 1) or up (step -1) from
 * index and wrapping round; index itself if there is none. */
static int menu_step(const ui_menu_item_t *items, int count, int index, int step)
{
    int i = index;
    int n;

    for (n = 0; n < count; n++) {
        i = (i + step + count) % count;
        if (!is_heading(&items[i]))
            return i;
    }
    return index;
}

/* Author: Thomas Dzubin */
int ui_menu(const char *title, const ui_menu_item_t *items, int count, const char *hint,
            int start)
{
    int chosen = start;
    int i;

    if (count > UI_MENU_MAX_ITEMS)
        count = UI_MENU_MAX_ITEMS;
    if (chosen < 0 || chosen >= count || is_heading(&items[chosen]))
        chosen = menu_step(items, count, count - 1, 1);     /* the first item */
    lcd_clear_screen();
    ui_put_row(UI_MENU_ROW_TITLE, title, COL_CYAN);
    ui_put_row(UI_MENU_ROW_HINT, hint, COL_YELLOW);
    for (i = 0; i < count; i++)
        menu_row(&items[i], i, i == chosen);

    for (;;) {
        uint8_t key = ui_wait_any_key();
        int before = chosen;

        if (key == KEY_ESC)
            return -1;
        if (key == KEY_ENTER || key == KEY_RETURN)
            return chosen;
        if (key == KEY_DOWN) {
            chosen = menu_step(items, count, chosen, 1);
        } else if (key == KEY_UP) {
            chosen = menu_step(items, count, chosen, -1);
        } else {
            for (i = 0; i < count; i++)
                if (items[i].key && ui_lower(key) == (uint8_t)items[i].key)
                    return i;
        }
        if (chosen != before) {
            menu_row(&items[before], before, false);
            menu_row(&items[chosen], chosen, true);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  The ways out                                                        */
/* ------------------------------------------------------------------ */
void ui_go_bootsel(void)
{
    plat_audio_stop();
    ui_put_row(UI_ROW_PROMPT, "REBOOTING TO BOOTSEL...", COL_YELLOW);
    plat_sleep_ms(UI_REBOOT_MS);
    plat_bootsel();
}

/* ESC or Q on the title screen */
void ui_go_loader(void)
{
    plat_audio_stop();
    ui_put_row(UI_ROW_EXIT, "BACK TO THE LOADER...", COL_YELLOW);
    plat_sleep_ms(UI_REBOOT_MS);
    plat_exit_to_loader();
}
