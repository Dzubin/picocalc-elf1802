/*
 * memview.c - the memory map screen. See memview.h.
 *
 * Author: Thomas Dzubin
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "lcd.h"          /* lcd_clear_screen()                               */

#include "elf1802_const.h"  /* the colours                                    */
#include "memview.h"
#include "rom.h"          /* the names of the ROM images                      */
#include "ui.h"

/* The name of the ROM image that starts at this address, or "". */
static const char *rom_name_at(uint32_t address)
{
    int i;

    for (i = 0; i < ROM_SLOTS; i++) {
        const rom_info_t *r = rom_info(i);

        if (r && r->fitted && r->address == address)
            return r->name;
    }
    return "";
}

/* Author: Thomas Dzubin */
void memview_run(const membus_t *bus)
{
    static const char *const names[] = MEMVIEW_KIND_NAMES;
    unsigned page = 0;
    int row = 0;

    lcd_clear_screen();
    ui_put_row(MEMVIEW_ROW_TITLE, MEMVIEW_TITLE, COL_CYAN);
    while (page < MEMBUS_PAGES && row < MEMVIEW_MAX_RUNS) {
        int kind = membus_page_kind(bus, page);
        unsigned end = page + 1;
        char line[64];                 /* ui_put_row() cuts it to the screen */
        unsigned kb10;

        while (end < MEMBUS_PAGES && membus_page_kind(bus, end) == kind)
            end++;
        kb10 = (end - page) * 10u / 4u;         /* 4 pages to the K, in tenths */
        snprintf(line, sizeof line, "%04X-%04X  %-9s %2u.%uK  %s",
                 page << MEMBUS_PAGE_SHIFT, (end << MEMBUS_PAGE_SHIFT) - 1u, names[kind],
                 kb10 / 10u, kb10 % 10u,
                 kind == MEMBUS_KIND_ROM ? rom_name_at(page << MEMBUS_PAGE_SHIFT) : "");
        ui_put_row(MEMVIEW_ROW_FIRST + row, line,
                   kind == MEMBUS_KIND_NONE ? COL_WHITE :
                   kind == MEMBUS_KIND_RAM ? COL_GREEN :
                   kind == MEMBUS_KIND_ROM ? COL_YELLOW : COL_CYAN);
        row++;
        page = end;
    }
    ui_put_row(MEMVIEW_ROW_HINT, MEMVIEW_HINT, COL_YELLOW);
    ui_wait_any_key();
}
