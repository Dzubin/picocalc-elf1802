/*
 * editor.c - the text editor and assembler screens. See editor.h.
 *
 * The text is one array, with the cursor as an index into it, which is plenty
 * quick for 12K of source on the PicoCalc: every line start is found by
 * scanning, so there is nothing else to keep in step.
 *
 * Author: Thomas Dzubin
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "lcd.h"          /* lcd_clear_screen(), lcd_putc(), RGB()            */
#include "keyboard.h"     /* KEY_* codes                                      */

#include "asm_core.h"
#include "isa.h"
#include "source_gen.h"
#include "editor.h"
#include "editor_const.h"
#include "elf1802_const.h"  /* the colours and TCOLS                          */
#include "filemenu.h"     /* the file chooser and name entry                  */
#include "helpscreen.h"   /* F1: the editor's help                            */
#include "platform.h"     /* the source files on the SD card                  */
#include "ui.h"

static char     text[EDITOR_MAX_TEXT + 1];      /* the source                 */
static int      length;                         /* characters in it           */
static int      cursor;                         /* the cursor, an index into it */
static int      top_line;                       /* the first line on the screen */
static int      left_col;                       /* the first column shown     */
static int      want_col;                       /* column to keep when moving up and down */
static bool     modified;                       /* changed since it was loaded or saved */
static bool     partial;                        /* the text is only the start of a program
                                                   that the RAM holds: assembling it
                                                   changes only the bytes it makes     */
static bool     keep_message;                   /* show the message on entering        */
static char     file_name[SAVE_NAME_MAX + 1];   /* the source's name, or empty */
static asm_t    assembler;                      /* the last assembly          */
static bool     result_valid;                   /* ... which is for the text as it is */
static int      error_next;                     /* the error E goes to next   */
static char     message[EDITOR_NOTE_MAX + 1];   /* the note on the message row */
static int      cursor_row_shown = -1;          /* the row the cursor was drawn on */
static const program_target_t *target;          /* the machine's memory, for loading and reading */
static editor_result_t *outcome;

static void set_message(const char *note)
{
    strncpy(message, note, EDITOR_NOTE_MAX);
    message[EDITOR_NOTE_MAX] = '\0';
}

/* ------------------------------------------------------------------ */
/*  The text                                                            */
/* ------------------------------------------------------------------ */
static int line_start(int pos)
{
    while (pos > 0 && text[pos - 1] != '\n')
        pos--;
    return pos;
}

static int line_end(int pos)
{
    while (pos < length && text[pos] != '\n')
        pos++;
    return pos;
}

static int line_number(int pos)
{
    int n = 0, i;

    for (i = 0; i < pos; i++)
        if (text[i] == '\n')
            n++;
    return n;
}

static int line_count(void)
{
    return line_number(length) + 1;
}

/* The index of the start of line n (from 0), or length if there is no such line. */
static int position_of_line(int n)
{
    int pos = 0;

    while (n > 0) {
        pos = line_end(pos);
        if (pos >= length)
            return length;
        pos++;
        n--;
    }
    return pos;
}

static int cursor_column(void)
{
    return cursor - line_start(cursor);
}

static void changed(void)
{
    modified = true;
    result_valid = false;
}

static bool insert_char(char c)
{
    if (length >= EDITOR_MAX_TEXT) {
        set_message("TEXT FULL");
        return false;
    }
    if (c != '\n' && line_end(cursor) - line_start(cursor) >= EDITOR_LINE_MAX) {
        set_message("LINE TOO LONG");
        return false;
    }
    memmove(text + cursor + 1, text + cursor, (size_t)(length - cursor + 1));
    text[cursor++] = c;
    length++;
    changed();
    return true;
}

static void delete_at(int pos)
{
    memmove(text + pos, text + pos + 1, (size_t)(length - pos));
    length--;
    changed();
}

/* Move the cursor up (negative) or down by lines, keeping the column it wants.
 *
 * Author: Thomas Dzubin */
static void move_vertical(int lines)
{
    int pos = cursor;
    int s, e;

    while (lines < 0) {
        s = line_start(pos);
        if (s == 0)
            break;
        pos = line_start(s - 1);
        lines++;
    }
    while (lines > 0) {
        e = line_end(pos);
        if (e >= length)
            break;
        pos = e + 1;
        lines--;
    }
    s = line_start(pos);
    e = line_end(pos);
    cursor = s + (want_col < e - s ? want_col : e - s);
}

static void clear_text(void)
{
    length = 0;
    text[0] = '\0';
    cursor = 0;
    top_line = 0;
    left_col = 0;
    want_col = 0;
    modified = false;
    partial = false;
    result_valid = false;
    file_name[0] = '\0';
}

/* ------------------------------------------------------------------ */
/*  The editing screen                                                  */
/* ------------------------------------------------------------------ */
/* Author: Thomas Dzubin */
static void draw(void)
{
    char row[TCOLS + 1];
    char status[EDITOR_FORMAT_BUF];
    int cursor_line = line_number(cursor);
    int col = cursor_column();
    int pos, r;
    bool exists = true;

    if (cursor_line < top_line)
        top_line = cursor_line;
    if (cursor_line >= top_line + EDITOR_TEXT_ROWS)
        top_line = cursor_line - EDITOR_TEXT_ROWS + 1;
    if (col < left_col)
        left_col = col;
    if (col >= left_col + TCOLS)
        left_col = col - TCOLS + 1;

    if (cursor_row_shown >= 0)          /* the cursor's cell is drawn over: redo the row */
        ui_forget_row_span(cursor_row_shown, cursor_row_shown);

    pos = position_of_line(top_line);
    for (r = 0; r < EDITOR_TEXT_ROWS; r++) {
        int line = top_line + r;
        uint16_t colour = COL_WHITE;
        int e = 0, i = 0;

        row[0] = '\0';
        if (exists) {
            e = line_end(pos);
            for (i = 0; i < TCOLS && pos + left_col + i < e; i++)
                row[i] = text[pos + left_col + i];
            row[i] = '\0';
            if (result_valid && line < assembler.line_count && (assembler.lines[line].flags & ASM_LINE_BAD))
                colour = COL_RED;
        }
        ui_panel_row(r, 0, row, colour);
        if (exists) {
            if (e < length)
                pos = e + 1;
            else
                exists = false;
        }
    }

    snprintf(status, sizeof status, "L%d/%d C%d  %d/%d  %s%s", cursor_line + 1,
             line_count(), col + 1, length, EDITOR_MAX_TEXT,
             file_name[0] ? file_name : "NEW", modified ? "*" : "");
    ui_panel_row(EDITOR_ROW_STATUS, 0, status, COL_CYAN);
    ui_panel_row(EDITOR_ROW_MESSAGE, 0, message[0] ? message : EDITOR_HINT,
                 message[0] ? COL_YELLOW : COL_WHITE);

    /* the cursor: the character under it, inverted */
    {
        char under = (cursor < length && text[cursor] != '\n') ? text[cursor] : ' ';

        cursor_row_shown = cursor_line - top_line;
        lcd_set_foreground(COL_BG);
        lcd_set_background(COL_WHITE);
        lcd_putc((uint8_t)(col - left_col), (uint8_t)cursor_row_shown, (uint8_t)under);
        lcd_set_background(COL_BG);
    }
}

/* Author: Thomas Dzubin */
static void edit_key(uint8_t key)
{
    int i, n;

    switch (key) {
    case KEY_UP:
        move_vertical(-1);
        break;
    case KEY_DOWN:
        move_vertical(1);
        break;
    case KEY_LEFT:
        if (cursor > 0)
            cursor--;
        want_col = cursor_column();
        break;
    case KEY_RIGHT:
        if (cursor < length)
            cursor++;
        want_col = cursor_column();
        break;
    case KEY_HOME:
        cursor = line_start(cursor);
        want_col = 0;
        break;
    case KEY_END:
        cursor = line_end(cursor);
        want_col = cursor_column();
        break;
    case KEY_PAGE_UP:
        move_vertical(-(EDITOR_TEXT_ROWS - 1));
        break;
    case KEY_PAGE_DOWN:
        move_vertical(EDITOR_TEXT_ROWS - 1);
        break;
    case KEY_ENTER:
    case KEY_RETURN:
        n = 0;                                  /* the new line gets the same indent */
        for (i = line_start(cursor); i < cursor && text[i] == ' '; i++)
            n++;
        if (insert_char('\n'))
            while (n-- > 0 && insert_char(' '))
                ;
        want_col = cursor_column();
        break;
    case KEY_BACKSPACE:
        if (cursor > 0) {
            cursor--;
            delete_at(cursor);
        }
        want_col = cursor_column();
        break;
    case KEY_DEL:
        if (cursor < length)
            delete_at(cursor);
        break;
    case KEY_TAB:
        n = EDITOR_TAB_WIDTH - cursor_column() % EDITOR_TAB_WIDTH;
        while (n-- > 0 && insert_char(' '))
            ;
        want_col = cursor_column();
        break;
    default:
        if (key >= 0x20 && key < 0x7F) {
            insert_char((char)key);
            want_col = cursor_column();
        }
        break;
    }
}

/* ------------------------------------------------------------------ */
/*  Assembling                                                          */
/* ------------------------------------------------------------------ */
static int run_assembler(void)
{
    int errors = asm_assemble(target->isa, &assembler, target->base, target->size, text,
                              (size_t)length);

    result_valid = true;
    error_next = 0;
    return errors;
}

/* Show error number i (from 0): the note, and the cursor on its line. */
static void show_error(int i)
{
    char note[EDITOR_FORMAT_BUF];
    const asm_error_t *e = &assembler.errors[i];

    snprintf(note, sizeof note, "LINE %d: %s", e->line, e->text);
    set_message(note);
    cursor = position_of_line(e->line - 1);
    want_col = 0;
}

/* Author: Thomas Dzubin */
static void cmd_assemble(void)
{
    char note[EDITOR_FORMAT_BUF];
    int errors;

    if (length == 0) {
        set_message("NOTHING TO ASSEMBLE");
        return;
    }
    errors = run_assembler();
    if (errors > 0) {
        show_error(0);
        error_next = 1;
        if (errors > 1) {
            snprintf(note, sizeof note, "%d ERRORS. LINE %d: %s", errors,
                     assembler.errors[0].line, assembler.errors[0].text);
            set_message(note);
        }
        return;
    }
    if (assembler.bytes == 0) {
        set_message("ASSEMBLED, BUT NO CODE WAS MADE");
        return;
    }
    if (partial) {                  /* only part of a program: keep the rest of the RAM */
        uint32_t i;

        for (i = 0; i < assembler.size; i++)
            if ((assembler.used[i >> 3] >> (i & 7)) & 1u)
                target->ram[i] = assembler.image[i];
        outcome->loaded = true;
        snprintf(outcome->note, sizeof outcome->note, "ASSEMBLED %u BYTES, REST KEPT",
                 (unsigned)assembler.bytes);
        snprintf(note, sizeof note, "OK: %u BYTES; THE REST OF RAM KEPT",
                 (unsigned)assembler.bytes);
        set_message(note);
        return;
    }
    program_target_load(target, assembler.image, ASM_IMAGE_SIZE);
    outcome->loaded = true;
    snprintf(outcome->note, sizeof outcome->note, "ASSEMBLED %u BYTES",
             (unsigned)assembler.bytes);
    if ((uint32_t)assembler.low != target->base)
        snprintf(note, sizeof note, "%u BYTES AT %04X-%04X: NOTHING AT %04X",
                 (unsigned)assembler.bytes, assembler.low, assembler.high,
                 (unsigned)target->base);
    else
        snprintf(note, sizeof note, "OK: %u BYTES AT %04X-%04X IN THE RAM",
                 (unsigned)assembler.bytes, assembler.low, assembler.high);
    set_message(note);
}

static void cmd_next_error(void)
{
    int shown;

    if (!result_valid || assembler.error_count == 0) {
        set_message("NO ERRORS TO SHOW (A ASSEMBLES)");
        return;
    }
    shown = assembler.error_count < ASM_MAX_ERRORS ? assembler.error_count : ASM_MAX_ERRORS;
    show_error(error_next % shown);
    error_next++;
}

/* ------------------------------------------------------------------ */
/*  The listing and the labels                                          */
/* ------------------------------------------------------------------ */
/* Index the symbols in order of address. */
static int sort_symbols(int *order)
{
    int n = assembler.symbol_count;
    int i, j;

    for (i = 0; i < n; i++)
        order[i] = i;
    for (i = 1; i < n; i++) {
        int tmp = order[i];

        for (j = i - 1; j >= 0 &&
                        assembler.symbols[order[j]].value > assembler.symbols[tmp].value; j--)
            order[j + 1] = order[j];
        order[j + 1] = tmp;
    }
    return n;
}

/* Author: Thomas Dzubin */
static void draw_listing(int top)
{
    int pos = position_of_line(top);
    int r;

    for (r = 0; r < LISTING_ROWS; r++) {
        int line = top + r;
        char row[TCOLS + 1];
        char bytes[12];
        int e;
        const asm_line_t *l;

        if (line >= assembler.line_count)
            break;
        l = &assembler.lines[line];
        e = line_end(pos);
        if ((l->flags & ASM_LINE_BAD)) {
            snprintf(row, sizeof row, "ERR   %.*s", (int)(TCOLS - 6), text + pos);
            row[6 + (e - pos < TCOLS - 6 ? e - pos : TCOLS - 6)] = '\0';
            ui_put_at(0, r, row, COL_RED);
        } else {
            int k;
            size_t n = 0;
            int shown = l->count < ASM_LISTING_BYTES ? (int)l->count : ASM_LISTING_BYTES;

            bytes[0] = '\0';
            for (k = 0; k < shown; k++)
                n += (size_t)snprintf(bytes + n, sizeof bytes - n, "%s%02X", k ? " " : "", l->bytes[k]);
            if (l->count > ASM_LISTING_BYTES)
                snprintf(bytes + n, sizeof bytes - n, "+");
            if (l->count > 0)
                snprintf(row, sizeof row, "%04X %-9s", l->address, bytes);
            else
                snprintf(row, sizeof row, "              ");
            ui_put_at(0, r, row, COL_GREEN);
            snprintf(row, sizeof row, "%.*s", (int)(TCOLS - LISTING_TEXT_COL), text + pos);
            row[e - pos < TCOLS - LISTING_TEXT_COL ? e - pos : TCOLS - LISTING_TEXT_COL] = '\0';
            ui_put_at(LISTING_TEXT_COL, r, row, COL_WHITE);
        }
        pos = e + 1;
    }
}

static void draw_symbols(int top, const int *order, int n)
{
    int r, c;

    for (r = 0; r < LISTING_ROWS; r++)
        for (c = 0; c < LISTING_SYMBOL_COLS; c++) {
            int i = (top + r) * LISTING_SYMBOL_COLS + c;
            char cell[24];

            if (i >= n)
                continue;
            snprintf(cell, sizeof cell, "%-12s %04X", assembler.symbols[order[i]].name,
                     assembler.symbols[order[i]].value);
            ui_put_at(c * 20, r, cell, COL_WHITE);
        }
}

/* Author: Thomas Dzubin */
static void listing_screen(void)
{
    static int order[ASM_MAX_SYMBOLS];
    int n = sort_symbols(order);
    bool symbols = false;
    int top = 0;

    for (;;) {
        char note[EDITOR_FORMAT_BUF];
        int last = symbols ? (n + LISTING_SYMBOL_COLS - 1) / LISTING_SYMBOL_COLS
                           : assembler.line_count;
        uint8_t key;

        lcd_clear_screen();
        if (symbols)
            draw_symbols(top, order, n);
        else
            draw_listing(top);
        if (assembler.error_count > 0) {
            snprintf(note, sizeof note, "%d ERRORS: THE FIRST IS LINE %d", assembler.error_count,
                     assembler.errors[0].line);
            ui_put_row(LISTING_ROW_HINT - 1, note, COL_RED);
        }
        ui_put_row(LISTING_ROW_HINT, symbols ? "ESC: back  TAB: listing  UP/DOWN"
                                             : "ESC: back  TAB: labels   UP/DOWN", COL_YELLOW);
        key = ui_read_key();
        if (key == KEY_ESC)
            return;
        if (key == KEY_TAB) {
            symbols = !symbols;
            top = 0;
        } else if (key == KEY_UP && top > 0) {
            top--;
        } else if (key == KEY_DOWN && top < last - 1) {
            top++;
        } else if (key == KEY_PAGE_UP) {
            top = top > LISTING_ROWS ? top - LISTING_ROWS : 0;
        } else if (key == KEY_PAGE_DOWN) {
            top = top + LISTING_ROWS < last ? top + LISTING_ROWS : (last > 0 ? last - 1 : 0);
        } else if (key == KEY_HOME) {
            top = 0;
        }
    }
}

static void cmd_listing(void)
{
    if (length == 0) {
        set_message("NOTHING TO LIST");
        return;
    }
    if (!result_valid)
        run_assembler();
    listing_screen();
}

/* ------------------------------------------------------------------ */
/*  Files, new text and the RAM as source                               */
/* ------------------------------------------------------------------ */
/* True when it is all right to throw the text away. */
static bool discard_ok(void)
{
    if (!modified)
        return true;
    lcd_clear_screen();
    ui_put_row(14, "THE TEXT HAS CHANGES.", COL_YELLOW);
    ui_put_row(16, "Throw them away?  Y: yes", COL_WHITE);
    ui_put_row(17, "any other key: no", COL_WHITE);
    return ui_lower(ui_wait_any_key()) == 'y';
}

/* Author: Thomas Dzubin */
static void cmd_save(void)
{
    char name[SAVE_NAME_MAX + 1];
    char file[PLAT_NAME_MAX];
    int rc;

    strcpy(name, file_name);
    if (!filemenu_ask_name("SAVE THE SOURCE ON THE SD CARD", PLAT_SOURCE_EXT, name, sizeof name))
        return;
    snprintf(file, sizeof file, "%s%s", name, PLAT_SOURCE_EXT);
    rc = plat_file_save(file, (const uint8_t *)text, (uint32_t)length);
    if (rc == PLAT_FILE_OK) {
        char note[EDITOR_NOTE_MAX + 1];

        strcpy(file_name, name);
        modified = false;
        snprintf(note, sizeof note, "SAVED %s", file);
        set_message(note);
    } else {
        set_message(filemenu_error_text(rc));
    }
}

/* Read a source file into the text, in place of the text there: no carriage
 * returns, tabs as spaces, nothing that cannot be shown. Returns the PLAT_FILE_
 * result; *cut is set if the file did not fit.
 *
 * Author: Thomas Dzubin */
static int read_source_file(const char *name, bool *cut)
{
    uint32_t got = 0;
    uint32_t i;
    int rc, col = 0;

    *cut = false;
    /* read into the assembler's image, which is free, and tidy it into the text */
    rc = plat_file_load(name, assembler.image, EDITOR_MAX_TEXT, &got);
    if (rc != PLAT_FILE_OK)
        return rc;
    if (got >= EDITOR_MAX_TEXT)         /* it is at least as big as the text */
        *cut = true;
    clear_text();
    for (i = 0; i < got; i++) {
        char c = (char)assembler.image[i];

        if (c == '\r')
            continue;
        if (length >= EDITOR_MAX_TEXT) {
            *cut = true;
            break;
        }
        if (c == '\t') {
            do {
                if (length >= EDITOR_MAX_TEXT)
                    break;
                text[length++] = ' ';
                col++;
            } while (col % EDITOR_TAB_WIDTH != 0);
            continue;
        }
        if (c == '\n') {
            text[length++] = c;
            col = 0;
        } else if (c >= 0x20 && c < 0x7F) {
            text[length++] = c;
            col++;
        }
    }
    text[length] = '\0';
    for (i = 0; i < SAVE_NAME_MAX && name[i] && name[i] != '.'; i++)
        file_name[i] = name[i];
    file_name[i] = '\0';
    return PLAT_FILE_OK;
}

static void cmd_open(void)
{
    char name[PLAT_NAME_MAX];
    bool cut;
    int rc;

    if (!discard_ok())
        return;
    if (!filemenu_pick_file("OPEN A SOURCE FILE", PLAT_KIND_SOURCE, name))
        return;
    rc = read_source_file(name, &cut);
    if (rc != PLAT_FILE_OK) {
        set_message(filemenu_error_text(rc));
        return;
    }
    set_message(cut ? "FILE TOO BIG: THE END WAS CUT OFF" : "OPENED");
}

static void cmd_new(void)
{
    if (!discard_ok())
        return;
    clear_text();
    set_message("NEW TEXT");
}

/* Author: Thomas Dzubin */
static void cmd_disassemble(void)
{
    uint32_t done = 0, limit = target->size;
    char note[EDITOR_NOTE_MAX + 1];
    size_t n;

    if (!discard_ok())
        return;
    while (limit > 0 && target->ram[limit - 1] == 0)
        limit--;
    clear_text();
    n = srcgen_from_image(target->isa, target->ram, target->size, text, sizeof text, &done);
    length = (int)n;
    modified = true;
    partial = limit > 0 && done < limit;
    if (limit == 0)
        set_message("THE RAM IS EMPTY");
    else if (done < limit) {
        snprintf(note, sizeof note, "LISTING STOPS AT %04X: TEXT FULL", (unsigned)done);
        set_message(note);
    } else {
        snprintf(note, sizeof note, "THE RAM AS SOURCE: %u BYTES", (unsigned)limit);
        set_message(note);
    }
}

/* ------------------------------------------------------------------ */
/*  The command menu                                                    */
/* ------------------------------------------------------------------ */
/* Assemble into the RAM and, if that worked, leave the editor for the Elf
 * screen with then (EDITOR_THEN_RUN or EDITOR_THEN_DEBUG) to do. With errors
 * the editor stays, showing the first one. Returns true when it is to leave.
 *
 * Author: Thomas Dzubin */
static bool assemble_then(int then)
{
    bool was_loaded = outcome->loaded;

    outcome->loaded = false;
    cmd_assemble();
    if (!outcome->loaded) {                 /* errors, or nothing to assemble */
        outcome->loaded = was_loaded;
        return false;
    }
    outcome->then = then;
    return true;
}

/* Returns true when the user chose to leave the editor.
 *
 * Author: Thomas Dzubin */
static bool command_menu(void)
{
    static const ui_menu_item_t items[] = EDITOR_MENU_ITEMS_TEXT;
    static int at;                          /* where the arrow was left */
    int pick = ui_menu(EDITOR_MENU_TITLE, items, (int)(sizeof items / sizeof items[0]),
                       EDITOR_MENU_HINT, at);

    if (pick < 0)
        return false;                       /* ESC: back to the text */
    at = pick;
    switch (pick) {
    case EDITOR_MENU_ASSEMBLE:    cmd_assemble();     break;
    case EDITOR_MENU_GO:          return assemble_then(EDITOR_THEN_RUN);
    case EDITOR_MENU_DEBUG:       return assemble_then(EDITOR_THEN_DEBUG);
    case EDITOR_MENU_LISTING:     cmd_listing();      break;
    case EDITOR_MENU_ERROR:       cmd_next_error();   break;
    case EDITOR_MENU_SAVE:        cmd_save();         break;
    case EDITOR_MENU_OPEN:        cmd_open();         break;
    case EDITOR_MENU_NEW:         cmd_new();          break;
    case EDITOR_MENU_DISASSEMBLE: cmd_disassemble();  break;
    default:                      return true;        /* leave the editor */
    }
    return false;
}

/* Author: Thomas Dzubin */
int editor_open_source(const program_target_t *t, const char *name, editor_result_t *result)
{
    bool cut;
    int rc;

    target = t;
    outcome = result;
    result->loaded = false;
    result->then = EDITOR_THEN_NONE;
    result->note[0] = '\0';
    rc = read_source_file(name, &cut);
    if (rc != PLAT_FILE_OK) {
        snprintf(result->note, sizeof result->note, "%s", filemenu_error_text(rc));
        return EDITOR_OPEN_FAILED;
    }
    keep_message = true;                /* what to show when the editor opens */
    if (cut) {
        set_message("FILE TOO BIG: THE END WAS CUT OFF");
        return EDITOR_OPEN_ERRORS;
    }
    if (length == 0) {
        set_message("THE FILE IS EMPTY");
        return EDITOR_OPEN_ERRORS;
    }
    if (run_assembler() > 0) {
        show_error(0);                  /* the first error, with the cursor on its line */
        error_next = 1;
        return EDITOR_OPEN_ERRORS;
    }
    if (assembler.bytes == 0) {
        set_message("ASSEMBLED, BUT NO CODE WAS MADE");
        return EDITOR_OPEN_ERRORS;
    }
    keep_message = false;
    program_target_load(t, assembler.image, ASM_IMAGE_SIZE);
    result->loaded = true;
    snprintf(result->note, sizeof result->note, "OPENED %s, %u BYTES", name,
             (unsigned)assembler.bytes);
    return EDITOR_OPEN_OK;
}

/* Author: Thomas Dzubin */
void editor_load_ram(const program_target_t *t, const char *name)
{
    uint32_t done = 0, limit = t->size;
    size_t i, k = 0;

    clear_text();
    length = (int)srcgen_from_image(t->isa, t->ram, t->size, text, sizeof text, &done);
    while (limit > 0 && t->ram[limit - 1] == 0)
        limit--;
    partial = done < limit;
    if (partial) {                  /* tell the user the listing is only the start */
        char note[EDITOR_NOTE_MAX + 1];

        snprintf(note, sizeof note, "LISTING STOPS AT %04X: TEXT FULL", (unsigned)done);
        set_message(note);
        keep_message = true;
    }
    for (i = 0; name[i] && name[i] != '.' && k < SAVE_NAME_MAX; i++) {
        char c = name[i];

        if (c >= 'a' && c <= 'z')
            c = (char)(c - 'a' + 'A');
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
            file_name[k++] = c;
    }
    file_name[k] = '\0';
    modified = false;
}

void editor_preload(const char *source, const char *name)
{
    size_t n = strlen(source);

    clear_text();
    if (n > EDITOR_MAX_TEXT)
        n = EDITOR_MAX_TEXT;
    memcpy(text, source, n);
    text[n] = '\0';
    length = (int)n;
    snprintf(file_name, sizeof file_name, "%s", name);
    modified = false;
}

/* Author: Thomas Dzubin */
void editor_run(const program_target_t *t, editor_result_t *result)
{
    target = t;
    outcome = result;
    result->loaded = false;
    result->then = EDITOR_THEN_NONE;
    result->note[0] = '\0';
    if (!keep_message)
        message[0] = '\0';
    keep_message = false;

    lcd_clear_screen();
    ui_forget_rows(0);
    cursor_row_shown = -1;
    for (;;) {
        uint8_t key;

        draw();
        key = ui_read_key();
        message[0] = '\0';
        if (key == KEY_F1) {
            help_screen_editor();
            lcd_clear_screen();         /* back to the text: draw it all again */
            ui_forget_rows(0);
            cursor_row_shown = -1;
            continue;
        }
        if (key == KEY_ESC) {
            if (command_menu())
                return;
            lcd_clear_screen();         /* back to the text: draw it all again */
            ui_forget_rows(0);
            cursor_row_shown = -1;
            continue;
        }
        edit_key(key);
    }
}
