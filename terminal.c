/*
 * terminal.c - the terminal screen. See terminal.h.
 *
 * Author: Thomas Dzubin
 */
#include <stdint.h>
#include <string.h>

#include "lcd.h"          /* RGB(), for the colours                           */

#include "elf1802_const.h"  /* the colours and TCOLS                          */
#include "terminal.h"
#include "ui.h"

static char cells[TERM_ROWS][TERM_COLS + 1];
static int  cursor_row, cursor_col;
static int  escape;                     /* 0: none, 1: after ESC, 2: in ESC [ ...  */
static int  parameter;                  /* the number in an escape sequence        */

void terminal_clear(void)
{
    int r;

    for (r = 0; r < TERM_ROWS; r++) {
        memset(cells[r], ' ', TERM_COLS);
        cells[r][TERM_COLS] = '\0';
    }
    cursor_row = cursor_col = 0;
    escape = 0;
}

static void scroll(void)
{
    memmove(cells[0], cells[1], sizeof cells - sizeof cells[0]);
    memset(cells[TERM_ROWS - 1], ' ', TERM_COLS);
}

static void line_feed(void)
{
    if (cursor_row + 1 < TERM_ROWS)
        cursor_row++;
    else
        scroll();
}

/* Author: Thomas Dzubin */
void terminal_put(uint8_t c)
{
    if (escape == 1) {                  /* ESC: only ESC [ is understood */
        escape = (c == '[') ? 2 : 0;
        parameter = 0;
        return;
    }
    if (escape == 2) {
        if (c >= '0' && c <= '9') {
            parameter = parameter * 10 + (c - '0');
        } else if (c >= 0x40 && c <= 0x7E) {    /* the letter that ends it */
            if (c == 'J' && parameter == 2)
                terminal_clear();
            else if (c == 'H')
                cursor_row = cursor_col = 0;
            escape = 0;
        }
        return;
    }

    switch (c) {
    case TERM_ESC:
        escape = 1;
        break;
    case TERM_CR:
        cursor_col = 0;
        break;
    case TERM_LF:
        line_feed();
        break;
    case TERM_BS:
        if (cursor_col > 0)
            cursor_col--;
        break;
    case TERM_TAB:
        cursor_col = (cursor_col / TERM_TAB_WIDTH + 1) * TERM_TAB_WIDTH;
        if (cursor_col >= TERM_COLS)
            cursor_col = TERM_COLS - 1;
        break;
    case TERM_FF:
        terminal_clear();
        break;
    default:
        if (c < ' ' || c > '~')
            break;                      /* other control characters do nothing */
        if (cursor_col >= TERM_COLS) {  /* wrap */
            cursor_col = 0;
            line_feed();
        }
        cells[cursor_row][cursor_col++] = (char)c;
        break;
    }
}

void terminal_forget(void)
{
    ui_forget_row_span(0, TERM_ROWS - 1);
}

void terminal_draw(void)
{
    int r;

    for (r = 0; r < TERM_ROWS; r++) {
        char line[TERM_COLS + 1];

        memcpy(line, cells[r], sizeof line);
        if (r == cursor_row && cursor_col < TERM_COLS && line[cursor_col] == ' ')
            line[cursor_col] = TERM_CURSOR;
        ui_panel_row(r, 0, line, COL_TERM_TEXT);
    }
}
