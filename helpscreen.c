/*
 * helpscreen.c - the help screen. See helpscreen.h.
 *
 * Author: Thomas Dzubin
 */
#include <stddef.h>

#include "lcd.h"          /* lcd_clear_screen()                               */

#include "elf1802_const.h"  /* the colours                                    */
#include "helpscreen.h"
#include "helpscreen_const.h"
#include "ui.h"

/* Show the lines (a heading starts with a hash) and wait for a key. */
static void show(const char *const *lines, size_t count)
{
    size_t i;

    lcd_clear_screen();
    for (i = 0; i < count; i++) {
        const char *text = lines[i];

        if (text[0] == '#')
            ui_put_at(0, HELP_ROW_FIRST + (int)i, text + 1, COL_CYAN);
        else
            ui_put_at(0, HELP_ROW_FIRST + (int)i, text, COL_WHITE);
    }
    ui_put_row(HELP_ROW_PROMPT, "PRESS ANY KEY TO GO BACK", COL_YELLOW);
    ui_wait_any_key();
}

void help_screen(void)
{
    static const char *const lines[] = HELP_LINES;

    show(lines, sizeof lines / sizeof lines[0]);
}

void help_screen_editor(void)
{
    static const char *const lines[] = HELP_EDITOR_LINES;

    show(lines, sizeof lines / sizeof lines[0]);
}
