/*
 * filemenu.c - the menu, saving and opening programs, and the file chooser and
 * name entry that the editor shares. See filemenu.h.
 *
 * Author: Thomas Dzubin
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "lcd.h"          /* lcd_clear_screen()                               */
#include "keyboard.h"     /* KEY_* codes                                      */

#include "elf1802_const.h"  /* the colours                                    */
#include "filemenu.h"
#include "filemenu_const.h"
#include "rom.h"          /* the ROM images                                   */
#include "source_gen.h"   /* the listing written by D                         */
#include "hexfile.h"      /* Intel HEX program files                          */
#include "platform.h"     /* the saved programs on the SD card                */
#include "ui.h"

#define KIND_COUNT          2               /* PLAT_KIND_PROGRAM, PLAT_KIND_SOURCE */

static uint8_t file_buf[HEX_FILE_MAX];          /* a .HEX file, while it is read */
static char    last_pick[KIND_COUNT][PLAT_NAME_MAX];   /* where each list was left */

/* True when the name ends in the extension, whatever the case. */
static bool ends_with(const char *name, const char *ext)
{
    size_t n = strlen(name);
    size_t e = strlen(ext);
    size_t i;

    if (n < e)
        return false;
    for (i = 0; i < e; i++)
        if (ui_lower((uint8_t)name[n - e + i]) != ui_lower((uint8_t)ext[i]))
            return false;
    return true;
}

const char *filemenu_error_text(int rc)
{
    switch (rc) {
    case PLAT_FILE_NO_CARD:   return "NO SD CARD (OR NOT FAT32)";
    case PLAT_FILE_NOT_FOUND: return "NOT FOUND";
    default:                  return "COULD NOT WRITE OR READ IT";
    }
}

/* Author: Thomas Dzubin */
bool filemenu_ask_name(const char *title, const char *ext, char *name, size_t size)
{
    char line[FILEMENU_NOTE_MAX + 1];
    size_t len = strlen(name);

    if (len > SAVE_NAME_MAX || size <= SAVE_NAME_MAX) {
        name[0] = '\0';
        len = 0;
    }
    for (;;) {
        lcd_clear_screen();
        ui_put_row(SAVE_ROW_TITLE, title, COL_CYAN);
        ui_put_row(SAVE_ROW_PROMPT, "File name (letters and digits):", COL_WHITE);
        snprintf(line, sizeof line, "%s_%s", name, ext);
        ui_put_row(SAVE_ROW_NAME, line, COL_GREEN);
        ui_put_row(SAVE_ROW_HINT, "ENTER: save   ESC: cancel", COL_YELLOW);

        uint8_t key = ui_wait_any_key();
        if (key == KEY_ESC)
            return false;
        if (key == KEY_BACKSPACE) {
            if (len > 0)
                name[--len] = '\0';
        } else if ((key == KEY_ENTER || key == KEY_RETURN) && len > 0) {
            return true;
        } else if (len < SAVE_NAME_MAX &&
                   ((key >= '0' && key <= '9') ||
                    (ui_lower(key) >= 'a' && ui_lower(key) <= 'z'))) {
            name[len++] = (key >= 'a' && key <= 'z') ? (char)(key - 'a' + 'A')
                                                     : (char)key;
            name[len] = '\0';
        }
    }
}

/* Author: Thomas Dzubin */
bool filemenu_pick_file(const char *title, int kind, char *name)
{
    static char names[LIST_MAX_FILES][PLAT_NAME_MAX];
    char *last = last_pick[kind == PLAT_KIND_SOURCE ? 1 : 0];
    int count = 0;
    int sel = 0, top = 0;
    int rc = plat_file_list(names, LIST_MAX_FILES, &count, kind);

    if (rc != PLAT_FILE_OK || count == 0) {
        lcd_clear_screen();
        ui_put_row(LIST_ROW_TITLE, title, COL_CYAN);
        ui_put_row(LIST_ROW_FIRST, rc != PLAT_FILE_OK && rc != PLAT_FILE_NOT_FOUND
                                       ? filemenu_error_text(rc)
                                       : "NOTHING FOUND", COL_RED);
        ui_put_row(LIST_ROW_FIRST + 2, "(in the ELF1802 folder on the card)", COL_WHITE);
        ui_wait_any_key();
        return false;
    }

    /* start on the file the list was left on last time, if it is still there */
    for (sel = count - 1; sel > 0 && strcmp(names[sel], last) != 0; sel--)
        ;
    top = sel - LIST_ROWS_SHOWN / 2;
    if (top > count - LIST_ROWS_SHOWN)
        top = count - LIST_ROWS_SHOWN;
    if (top < 0)
        top = 0;

    for (;;) {
        int i;

        lcd_clear_screen();
        ui_put_row(LIST_ROW_TITLE, title, COL_CYAN);
        for (i = 0; i < LIST_ROWS_SHOWN && top + i < count; i++) {
            ui_put_at(LIST_COL_NAME - 2, LIST_ROW_FIRST + i,
                      top + i == sel ? ">" : " ", COL_YELLOW);
            ui_put_at(LIST_COL_NAME, LIST_ROW_FIRST + i, names[top + i],
                      top + i == sel ? COL_YELLOW : COL_WHITE);
        }
        ui_put_row(LIST_ROW_HINT, "UP/DOWN  ENTER: open  ESC: cancel", COL_YELLOW);

        uint8_t key = ui_wait_any_key();
        strcpy(last, names[sel]);               /* the file the list is left on */
        if (key == KEY_ESC)
            return false;
        if (key == KEY_UP && sel > 0)
            sel--;
        else if (key == KEY_DOWN && sel < count - 1)
            sel++;
        else if (key == KEY_ENTER || key == KEY_RETURN) {
            strcpy(name, names[sel]);
            return true;
        }
        if (sel < top)
            top = sel;
        if (sel >= top + LIST_ROWS_SHOWN)
            top = sel - LIST_ROWS_SHOWN + 1;
    }
}

/* ------------------------------------------------------------------ */
/*  ROM images                                                          */
/* ------------------------------------------------------------------ */
/* The text of a ROM error. */
static const char *rom_error_text(int rc)
{
    switch (rc) {
    case ROM_ERR_ADDRESS:   return "ADDRESS: 4 HEX DIGITS ENDING 00";
    case ROM_ERR_EMPTY:     return "THE FILE IS EMPTY";
    case ROM_ERR_SLOT:      return "NO SUCH SLOT";
    default:                return filemenu_error_text(rc);
    }
}

/* One slot: choose a file and an address for it, or take its ROM out.
 *
 * Author: Thomas Dzubin */
static void rom_slot_screen(const program_target_t *t, int slot, filemenu_result_t *result)
{
    for (;;) {
        char line[TCOLS + 1];
        const rom_info_t *r = rom_info(slot);

        lcd_clear_screen();
        snprintf(line, sizeof line, "ROM SLOT %d", slot + 1);
        ui_put_row(ROMUI_ROW_TITLE, line, COL_CYAN);
        if (r->fitted)
            snprintf(line, sizeof line, "%s AT %04X, %u BYTES", r->name, (unsigned)r->address,
                     (unsigned)r->size);
        else
            snprintf(line, sizeof line, "NOTHING FITTED");
        ui_put_row(ROMUI_ROW_FIRST, line, COL_WHITE);
        ui_put_row(ROMUI_ROW_HINT - 2, "ENTER: CHOOSE A .ROM FILE", COL_YELLOW);
        ui_put_row(ROMUI_ROW_HINT - 1, "DEL: TAKE IT OUT", COL_YELLOW);
        ui_put_row(ROMUI_ROW_HINT, "ESC: BACK", COL_YELLOW);

        uint8_t key = ui_wait_any_key();
        if (key == KEY_ESC)
            return;
        if (key == KEY_DEL || key == KEY_BACKSPACE) {
            rom_clear(slot);
            if (t->remap)
                t->remap(t->ctx);
            rom_config_save();
            snprintf(result->note, sizeof result->note, "ROM %d TAKEN OUT", slot + 1);
        } else if (key == KEY_ENTER || key == KEY_RETURN) {
            char name[PLAT_NAME_MAX];
            char text[SAVE_NAME_MAX + 1];
            unsigned address;
            const char *stop;
            int rc;

            if (!filemenu_pick_file("CHOOSE A ROM IMAGE", PLAT_KIND_ROM, name))
                continue;
            if (r->fitted)
                snprintf(text, sizeof text, "%04X", (unsigned)r->address);
            else
                text[0] = '\0';
            if (!filemenu_ask_name("ADDRESS OF THE ROM (HEX)", "", text, sizeof text))
                continue;
            address = 0;
            rc = text[0] ? ROM_OK : ROM_ERR_ADDRESS;
            for (stop = text; *stop && rc == ROM_OK; stop++) {          /* hex digits only */
                char d = *stop;

                if (d >= '0' && d <= '9')
                    address = address * 16u + (unsigned)(d - '0');
                else if (d >= 'A' && d <= 'F')
                    address = address * 16u + (unsigned)(d - 'A' + 10);
                else if (d >= 'a' && d <= 'f')
                    address = address * 16u + (unsigned)(d - 'a' + 10);
                else
                    rc = ROM_ERR_ADDRESS;
            }
            if (rc == ROM_OK && strlen(text) > ROMUI_ADDRESS_DIGITS)
                rc = ROM_ERR_ADDRESS;
            if (rc == ROM_OK)
                rc = rom_set(slot, name, address);
            if (rc != ROM_OK) {
                ui_put_row(ROMUI_ROW_NOTE, rom_error_text(rc), COL_RED);
                ui_wait_any_key();
                continue;
            }
            if (t->remap)
                t->remap(t->ctx);
            rom_config_save();
            snprintf(result->note, sizeof result->note, "ROM %d: %s AT %04X", slot + 1, name,
                     (unsigned)address);
        }
    }
}

/* The ROM images: a menu of the slots.
 *
 * Author: Thomas Dzubin */
static void rom_screen(const program_target_t *t, filemenu_result_t *result)
{
    char labels[ROM_SLOTS][MENU_LABEL_MAX];
    ui_menu_item_t items[ROM_SLOTS];
    int at = 0;

    for (;;) {
        int i, pick;

        for (i = 0; i < ROM_SLOTS; i++) {
            const rom_info_t *r = rom_info(i);

            if (r->fitted)
                snprintf(labels[i], sizeof labels[i], "Slot %d: %s AT %04X", i + 1, r->name,
                         (unsigned)r->address);
            else
                snprintf(labels[i], sizeof labels[i], "Slot %d: (empty)", i + 1);
            items[i].text = labels[i];
            items[i].key = 0;
        }
        pick = ui_menu("ROM IMAGES", items, ROM_SLOTS, MENU_ROM_HINT, at);
        if (pick < 0)
            return;
        at = pick;
        rom_slot_screen(t, pick, result);
    }
}

static char last_name[SAVE_NAME_MAX + 1];           /* the program saved or opened last */

/* Remember a file name without its extension (letters and digits, as many as a
 * name may have) for the next save to offer. */
static void remember(const char *file)
{
    size_t i, k = 0;

    for (i = 0; file[i] && file[i] != '.' && k < SAVE_NAME_MAX; i++)
        if ((file[i] >= 'A' && file[i] <= 'Z') || (file[i] >= 'a' && file[i] <= 'z') ||
            (file[i] >= '0' && file[i] <= '9'))
            last_name[k++] = (file[i] >= 'a' && file[i] <= 'z') ? (char)(file[i] - 'a' + 'A')
                                                                : file[i];
    last_name[k] = '\0';
}

/* Ask for a name and write the whole RAM as assembly source to NAME.ASM, a
 * piece at a time, so a program of any size fits. Returns true if it was
 * written (the note says so).
 *
 * Author: Thomas Dzubin */
static bool listing_screen(const program_target_t *t, filemenu_result_t *result)
{
    char name[SAVE_NAME_MAX + 1];
    char *chunk = (char *)file_buf;

    snprintf(name, sizeof name, "%s", last_name);
    for (;;) {
        char file[PLAT_NAME_MAX];
        uint32_t total = 0;
        bool finished = false;
        int rc;

        if (!filemenu_ask_name("WRITE THE RAM AS AN .ASM FILE", PLAT_SOURCE_EXT, name,
                               sizeof name))
            return false;
        snprintf(file, sizeof file, "%s%s", name, PLAT_SOURCE_EXT);
        ui_put_row(SAVE_ROW_RESULT, "WRITING...", COL_YELLOW);
        rc = plat_file_begin(file);
        if (rc == PLAT_FILE_OK) {
            srcgen_listing_start(t->isa, t->ram, t->size);
            while (!finished && rc == PLAT_FILE_OK) {
                size_t n = srcgen_listing_next(chunk, LISTING_CHUNK, &finished);

                rc = plat_file_write((const uint8_t *)chunk, (uint32_t)n);
                total += (uint32_t)n;
            }
            if (plat_file_end() != PLAT_FILE_OK && rc == PLAT_FILE_OK)
                rc = PLAT_FILE_FAILED;
        }
        if (rc == PLAT_FILE_OK) {
            snprintf(result->note, sizeof result->note, "WROTE %s, %u BYTES", file,
                     (unsigned)total);
            return true;
        }
        ui_put_row(SAVE_ROW_RESULT, filemenu_error_text(rc), COL_RED);
        ui_wait_any_key();
    }
}

/* Ask for a name and write the RAM image to NAME.BIN. Returns true if it
 * was saved (the note says so).
 *
 * Author: Thomas Dzubin */
static bool save_screen(const program_target_t *t, filemenu_result_t *result)
{
    char name[SAVE_NAME_MAX + 1];

    name[0] = '\0';
    for (;;) {
        char file[PLAT_NAME_MAX];
        int rc;

        if (!filemenu_ask_name("SAVE THE RAM TO THE SD CARD", PLAT_PROGRAM_EXT,
                               name, sizeof name))
            return false;
        snprintf(file, sizeof file, "%s%s", name, PLAT_PROGRAM_EXT);
        rc = plat_file_save(file, t->ram, t->size);
        if (rc == PLAT_FILE_OK) {
            remember(file);
            snprintf(result->note, sizeof result->note, "SAVED %s", file);
            return true;
        }
        ui_put_row(SAVE_ROW_RESULT, filemenu_error_text(rc), COL_RED);
        ui_wait_any_key();
    }
}

/* List the programs on the card (.BIN images and .HEX files); ENTER loads the
 * chosen one into the RAM. Returns true if one was opened (the result says so
 * and has the note).
 *
 * Author: Thomas Dzubin */
static bool open_screen(const program_target_t *t, filemenu_result_t *result)
{
    for (;;) {
        char name[PLAT_NAME_MAX];
        uint32_t len = 0;
        int rc;

        if (!filemenu_pick_file("OPEN A PROGRAM", PLAT_KIND_PROGRAM, name))
            return false;
        remember(name);

        if (ends_with(name, PLAT_SOURCE_EXT)) {         /* the editor takes this one */
            snprintf(result->source, sizeof result->source, "%s", name);
            return true;
        }
        if (ends_with(name, PLAT_HEX_EXT)) {
            uint32_t stored = 0, skipped = 0;
            int hrc;

            rc = plat_file_load(name, file_buf, HEX_FILE_MAX, &len);
            if (rc == PLAT_FILE_OK) {
                program_target_load(t, NULL, 0);        /* clear it first */
                hrc = hex_load((const char *)file_buf, len, t->ram,
                               t->size, &stored, &skipped);
                if (hrc != HEX_OK) {
                    ui_put_row(LIST_ROW_HINT - 2, hrc == HEX_ERR_CHECKSUM ?
                               "BAD CHECKSUM IN THE HEX FILE" :
                               "NOT A GOOD HEX FILE (OR TOO LONG)", COL_RED);
                    ui_wait_any_key();
                    continue;
                }
                if (skipped > 0)
                    snprintf(result->note, sizeof result->note,
                             "OPENED %s, %u BYTES NOT IN RAM", name,
                             (unsigned)skipped);
                else
                    snprintf(result->note, sizeof result->note, "OPENED %s", name);
                snprintf(result->name, sizeof result->name, "%s", name);
                return true;
            }
        } else {
            rc = plat_file_load(name, t->ram, t->size, &len);
            if (rc == PLAT_FILE_OK) {
                program_target_load(t, t->ram, len);    /* the rest cleared, the machine told */
                snprintf(result->note, sizeof result->note, "OPENED %s", name);
                snprintf(result->name, sizeof result->name, "%s", name);
                return true;
            }
        }
        ui_put_row(LIST_ROW_HINT - 2, filemenu_error_text(rc), COL_RED);
        ui_wait_any_key();
    }
}

/* Ask before something that cannot be undone. Only Y says yes; any other key,
 * ESC too, says no. */
static bool confirm(const char *question)
{
    lcd_clear_screen();
    ui_put_row(CONFIRM_ROW, question, COL_YELLOW);
    ui_put_row(CONFIRM_ROW + 2, "Y: yes", COL_WHITE);
    ui_put_row(CONFIRM_ROW + 3, "any other key: no", COL_WHITE);
    return ui_lower(ui_wait_any_key()) == 'y';
}

/* The rows of the menu, by what they lead to. */
enum {
    ID_SAVE, ID_OPEN, ID_NEW, ID_LISTING, ID_ROM,
    ID_EDIT, ID_DEBUGGER, ID_HELP,
    ID_PROTECT, ID_KEYBOARD, ID_SOUND, ID_VIEW, ID_EXTRA,
    ID_TITLE,
    ID_COUNT
};

/* Add a row; id is what it leads to (-1 for a heading). */
static void add_row(ui_menu_item_t *items, int *ids, int *n, const char *text, char key, int id)
{
    items[*n].text = text;
    items[*n].key = key;
    ids[*n] = id;
    (*n)++;
}

/* The rows of the menu, with the state of the switches in the rows that show
 * it. Letters are only used where they do not clash with the keypad screen's
 * keys (0-9, A-F, R, L, M, I): the rows without a letter are chosen with the
 * arrow keys. Returns how many rows there are.
 *
 * Author: Thomas Dzubin */
static int build_menu(const program_target_t *t, const filemenu_machine_t *m,
                      char labels[ID_COUNT][MENU_LABEL_MAX], ui_menu_item_t *items, int *ids)
{
    int n = 0;

    snprintf(labels[ID_PROTECT], MENU_LABEL_MAX, "   Memory protect: %s", m->mp_on ? "ON" : "OFF");
    snprintf(labels[ID_KEYBOARD], MENU_LABEL_MAX, "K  Keyboard: %s  (F4)",
             m->ascii_mode ? "ASCII" : "HEX KEYPAD");
    snprintf(labels[ID_SOUND], MENU_LABEL_MAX, "Q  Sound: %s", m->sound_on ? "ON" : "OFF");
    snprintf(labels[ID_VIEW], MENU_LABEL_MAX, "V  Top of screen: %s", m->vdu_shown ? "VDU" : "PICTURE");
    if (t->extra_key)               /* something of the machine's own (the 1861) */
        snprintf(labels[ID_EXTRA], MENU_LABEL_MAX, "%c  %s", t->extra_key - 'a' + 'A',
                 t->extra_text(t->ctx));

    add_row(items, ids, &n, "#FILES (SD CARD)", 0, -1);
    add_row(items, ids, &n, "S  Save the RAM to the SD card", 's', ID_SAVE);
    add_row(items, ids, &n, "O  Open a .BIN, .HEX or .ASM file", 'o', ID_OPEN);
    add_row(items, ids, &n, "N  New: clear the RAM", 'n', ID_NEW);
    add_row(items, ids, &n, "W  Write a listing file (.ASM)", 'w', ID_LISTING);
    if (t->bus)                         /* the .ROM files come from the SD card */
        add_row(items, ids, &n, "   ROM images (from the SD card)", 0, ID_ROM);
    add_row(items, ids, &n, "#", 0, -1);                /* a blank row between groups */
    add_row(items, ids, &n, "#TOOLS", 0, -1);
    add_row(items, ids, &n, "P  Program editor and assembler", 'p', ID_EDIT);
    add_row(items, ids, &n, "G  Debugger", 'g', ID_DEBUGGER);
    add_row(items, ids, &n, "H  Help  (F1)", 'h', ID_HELP);
    add_row(items, ids, &n, "#", 0, -1);
    add_row(items, ids, &n, "#MACHINE", 0, -1);
    add_row(items, ids, &n, labels[ID_PROTECT], 0, ID_PROTECT);
    add_row(items, ids, &n, labels[ID_KEYBOARD], 'k', ID_KEYBOARD);
    add_row(items, ids, &n, labels[ID_SOUND], 'q', ID_SOUND);
    add_row(items, ids, &n, labels[ID_VIEW], 'v', ID_VIEW);
    if (t->extra_key)
        add_row(items, ids, &n, labels[ID_EXTRA], t->extra_key, ID_EXTRA);
    add_row(items, ids, &n, "#", 0, -1);
    add_row(items, ids, &n, "T  Title screen", 't', ID_TITLE);
    return n;
}

/* Author: Thomas Dzubin */
void filemenu_run(const program_target_t *t, const filemenu_machine_t *m,
                  filemenu_result_t *result)
{
    char labels[ID_COUNT][MENU_LABEL_MAX];
    ui_menu_item_t items[MENU_MAX_ROWS];
    int ids[MENU_MAX_ROWS];
    int cursor = 0;

    result->action = FILEMENU_ACT_NONE;
    result->leave = result->opened = result->cleared = result->video_changed = false;
    result->edit = false;
    result->note[0] = '\0';
    result->name[0] = '\0';
    result->source[0] = '\0';

    for (;;) {
        int count = build_menu(t, m, labels, items, ids);
        int pick = ui_menu(MENU_TITLE, items, count, MENU_HINT, cursor);

        if (pick < 0)
            return;                     /* ESC: back to the Elf */
        cursor = pick;
        switch (ids[pick]) {
        case ID_SAVE:
            if (save_screen(t, result))
                return;                 /* saved: back to the Elf */
            break;
        case ID_OPEN:
            if (open_screen(t, result)) {
                result->opened = result->source[0] == '\0';
                return;                 /* opened: back to the Elf */
            }
            break;
        case ID_NEW:
            if (!confirm("CLEAR THE WHOLE RAM?"))
                break;
            program_target_load(t, NULL, 0);
            result->cleared = true;
            snprintf(result->note, sizeof result->note, "RAM CLEARED");
            return;
        case ID_LISTING:
            if (listing_screen(t, result))
                return;                 /* written: back to the Elf */
            break;
        case ID_EDIT:
            result->edit = true;        /* the caller runs the editor */
            return;
        case ID_DEBUGGER:
            result->action = FILEMENU_ACT_DEBUGGER;
            return;
        case ID_HELP:
            result->action = FILEMENU_ACT_HELP;
            return;
        case ID_PROTECT:
            result->action = FILEMENU_ACT_PROTECT;
            return;
        case ID_KEYBOARD:
            result->action = FILEMENU_ACT_KEYBOARD;
            return;
        case ID_SOUND:
            result->action = FILEMENU_ACT_SOUND;
            return;
        case ID_VIEW:
            result->action = FILEMENU_ACT_VIEW;
            return;
        case ID_EXTRA:
            if (t->extra_press(t->ctx, result->note, sizeof result->note))
                result->video_changed = true;       /* the screen has to be drawn again */
            break;
        case ID_ROM:
            rom_screen(t, result);
            break;
        default:                        /* ID_TITLE */
            result->leave = true;
            return;
        }
    }
}
