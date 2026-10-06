/*
 * title.c - the title screen. See title.h.
 *
 * Author: Thomas Dzubin
 */
#include "lcd.h"          /* lcd_clear_screen()                               */

#include "elf1802_const.h"  /* VERSION and the colours                        */
#include "title.h"
#include "title_const.h"
#include "ui.h"
#include "ui_const.h"

void title_screen(void)
{
    lcd_clear_screen();
    ui_put_row(SPLASH_ROW_TITLE, "ELF1802", COL_CYAN);
    ui_put_row(SPLASH_ROW_VERSION, VERSION, COL_CYAN);
    ui_put_row(SPLASH_ROW_ABOUT_1, "A COSMAC ELF: RCA 1802 CPU,", COL_WHITE);
    ui_put_row(SPLASH_ROW_ABOUT_2, "16K RAM and the 1861 video chip", COL_WHITE);
    ui_put_row(SPLASH_ROW_AUTHOR, "BY THOMAS DZUBIN", COL_WHITE);
    ui_put_row(UI_ROW_PROMPT, "PRESS ANY KEY TO START", COL_WHITE);
    ui_put_row(UI_ROW_EXIT, "or ESC / Q to exit", COL_WHITE);
    ui_put_row(SPLASH_ROW_BOOT, "B: start with the boot program again", COL_WHITE);
    ui_put_row(SPLASH_ROW_KEYS, SPLASH_KEYS_TEXT, COL_YELLOW);
}
