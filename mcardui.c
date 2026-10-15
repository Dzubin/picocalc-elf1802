/*
 * mcardui.c - the Membership Card's screen and keys. See mcardui.h.
 *
 * Author: Thomas Dzubin
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "lcd.h"          /* lcd_clear_screen()                               */
#include "keyboard.h"     /* KEY_* codes                                      */

#include "elf1802_const.h"  /* the colours, TCOLS, KEY_POLL_MS and so on       */
#include "filemenu.h"     /* the ROM images screen                            */
#include "mcard.h"
#include "mcard_monitor.h"  /* the card's own serial monitor                  */
#include "mcardui.h"
#include "mcardui_const.h"
#include "memview.h"      /* the memory map                                   */
#include "platform.h"     /* the clock                                        */
#include "terminal.h"
#include "ui.h"

static mcard_t card;
static program_target_t target;                 /* its RAM, for the ROM screen */
static bool     powered;                        /* the card has been switched on once */
static bool     typing;                         /* keys go down the serial line */
static int      baud_step = 1;                  /* the step of SERIAL_BAUDS in use (4800) */
static bool     key_down[256];                  /* keys held, by lowercase code */
static uint32_t key_press_ms[256];
static char     message[TCOLS + 1];
static uint32_t message_until_ms;

static void set_message(const char *text)
{
    strncpy(message, text, TCOLS);
    message[TCOLS] = '\0';
    message_until_ms = plat_now_ms() + MESSAGE_MS;
}

/* ------------------------------------------------------------------ */
/*  The front panel                                                     */
/* ------------------------------------------------------------------ */
static const char *mode_name(void)
{
    switch (card.cpu.mode) {
    case CPU_MODE_RUN:   return "RUN";
    case CPU_MODE_PAUSE: return "WAIT";
    case CPU_MODE_RESET: return "CLEAR";
    default:             return "LOAD";
    }
}

/* Eight bits as "* . * ..." (a lit LED or a switch up is a star). */
static void bit_text(char *out, size_t size, uint8_t value)
{
    int bit;
    size_t n = 0;

    for (bit = 7; bit >= 0 && n + 2 < size; bit--) {
        out[n++] = ((value >> bit) & 1) ? '*' : '.';
        out[n++] = ' ';
    }
    out[n > 0 ? n - 1 : 0] = '\0';
}

/* Bring the panel rows up to date; ui_panel_row() writes only what changed. */
static void panel_update(void)
{
    char line[64];                      /* ui_panel_row() cuts it to the screen */
    char bits[24];
    uint32_t now_ms = plat_now_ms();

    ui_panel_row(MCUI_ROW_TITLE, 0, MCUI_TITLE, COL_CYAN);

    snprintf(line, sizeof line, "DATA LEDS   %s    HEX", MCUI_HEAD);
    ui_panel_row(MCUI_ROW_LED_HEAD, 0, line, COL_WHITE);
    bit_text(bits, sizeof bits, card.display);
    snprintf(line, sizeof line, "OUT 4       %s     %02X", bits, card.display);
    ui_panel_row(MCUI_ROW_LED, 0, line, COL_YELLOW);

    snprintf(line, sizeof line, "DATA SWITCHES %s  (0-7)", MCUI_HEAD);
    ui_panel_row(MCUI_ROW_SW_HEAD, 0, line, COL_WHITE);
    bit_text(bits, sizeof bits, card.data_switches);
    snprintf(line, sizeof line, "INP 4       %s     %02X", bits, card.data_switches);
    ui_panel_row(MCUI_ROW_SW, 0, line, COL_GREEN);

    snprintf(line, sizeof line, "Q [%c]  EF3 [%c]   MODE %s", card.cpu.q ? '*' : ' ',
             serial_flag_high(&card.serial) ? ' ' : '*', mode_name());
    ui_panel_row(MCUI_ROW_STATUS, 0, line, COL_WHITE);
    snprintf(line, sizeof line, "WAIT[W] %-4s CLR[C] %-4s WRITE[R] %s",
             card.sw_wait_up ? "UP" : "DOWN", card.sw_clr_up ? "UP" : "DOWN",
             card.sw_write ? "UP" : "DOWN");
    ui_panel_row(MCUI_ROW_SWITCHES, 0, line, COL_WHITE);
    snprintf(line, sizeof line, "IN [ENTER] %-5s  KEYS: %s", card.in_button ? "DOWN" : "UP",
             typing ? "TERMINAL" : "PANEL");
    ui_panel_row(MCUI_ROW_IN, 0, line, typing ? COL_YELLOW : COL_WHITE);

    if (typing) {
        ui_panel_row(MCUI_ROW_HELP_1, 0, "TERMINAL KEYS: typed to the card", COL_YELLOW);
        ui_panel_row(MCUI_ROW_HELP_2, 0, "F4: panel keys again   F5: sends ESC", COL_WHITE);
    } else {
        ui_panel_row(MCUI_ROW_HELP_1, 0, "0-7 data switch  W C R  ENTER = IN", COL_WHITE);
        ui_panel_row(MCUI_ROW_HELP_2, 0, "F4: type to the card   ESC: menu", COL_WHITE);
    }
    if (message[0] && now_ms >= message_until_ms)
        message[0] = '\0';
    ui_panel_row(MCUI_ROW_MESSAGE, 0, message, COL_YELLOW);
}

static void screen_setup(void)
{
    lcd_clear_screen();
    ui_forget_rows(0);
    terminal_forget();
}

/* ------------------------------------------------------------------ */
/*  Keys                                                                */
/* ------------------------------------------------------------------ */
/* Note a key event; true when it is a press of a key that is already down. */
static bool key_repeat(uint8_t code, uint8_t state)
{
    uint8_t id = ui_lower(code);
    uint32_t now_ms = plat_now_ms();
    bool repeat = false;

    if (state == KEY_STATE_PRESSED) {
        repeat = key_down[id] && now_ms - key_press_ms[id] < KEY_STALE_MS;
        key_down[id] = true;
        key_press_ms[id] = now_ms;
    } else if (state == KEY_STATE_RELEASED) {
        key_down[id] = false;
    }
    return repeat;
}

/* A typed key goes down the serial line: ENTER as CR, the printable keys (in
 * capitals), BACKSPACE. */
static void typed_key(uint8_t code)
{
    uint8_t out = 0;

    if (code == KEY_ENTER || code == KEY_RETURN)
        out = ASCII_ENTER;
    else if (code == KEY_BACKSPACE)
        out = ASCII_BACKSPACE;
    else if (code >= 0x20 && code < 0x7F)
        out = code;
    if (ASCII_UPPERCASE_ONLY && out >= 'a' && out <= 'z')
        out = (uint8_t)(out - 'a' + 'A');
    if (out)
        mcard_key(&card, out);
}

/* One key event. Returns true when ESC asks for the menu. */
static bool handle_key(uint16_t e)
{
    uint8_t code = ui_ev_code(e);
    uint8_t state = ui_ev_state(e);
    uint8_t key = ui_lower(code);

    if (state == KEY_STATE_PRESSED && code == '~')
        ui_go_bootsel();
    if (ui_is_mod_key(code))
        return false;
    if (key_repeat(code, state) && !(typing && code < 0x80))
        return false;

    if (state == KEY_STATE_PRESSED && code == FKEY_ASCII) {
        typing = !typing;
        set_message(typing ? "TERMINAL KEYS: F4 FOR THE PANEL" : "PANEL KEYS");
        return false;
    }
    if (state == KEY_STATE_PRESSED && code == FKEY_ESC) {
        mcard_key(&card, ASCII_ESCAPE);
        return false;
    }
    if (state == KEY_STATE_PRESSED && code == FKEY_HELP) {
        set_message("0-7 SWITCHES, ENTER IN, ESC MENU");
        return false;
    }
    if (state == KEY_STATE_PRESSED && code == KEY_ESC)
        return true;
    if (typing) {
        if (state == KEY_STATE_PRESSED)
            typed_key(code);
        return false;
    }

    if (key == MCUI_KEY_IN || code == KEY_ENTER || code == KEY_RETURN) {
        if (state == KEY_STATE_PRESSED)
            mcard_in_button(&card, true);
        else if (state == KEY_STATE_RELEASED)
            mcard_in_button(&card, false);
        return false;
    }
    if (state != KEY_STATE_PRESSED)
        return false;
    if (code >= '0' && code <= '7')
        mcard_data_switch(&card, (unsigned)(code - '0'));
    else if (key == MCUI_KEY_WAIT)
        mcard_set_switches(&card, !card.sw_wait_up, card.sw_clr_up, card.sw_write);
    else if (key == MCUI_KEY_CLR)
        mcard_set_switches(&card, card.sw_wait_up, !card.sw_clr_up, card.sw_write);
    else if (key == MCUI_KEY_WRITE)
        mcard_set_switches(&card, card.sw_wait_up, card.sw_clr_up, !card.sw_write);
    return false;
}

/* ------------------------------------------------------------------ */
/*  The menu                                                            */
/* ------------------------------------------------------------------ */
static uint32_t baud_now(void)
{
    static const uint32_t bauds[SERIAL_BAUD_STEPS] = SERIAL_BAUDS;

    return bauds[baud_step];
}

/* The card's menu. Returns true to go back to the title screen. */
static bool menu(void)
{
    enum { M_SPEED, M_POLARITY, M_ROMS, M_MAP, M_CLEAR, M_KEYS, M_POWER, M_TITLE, M_COUNT };
    char labels[M_COUNT][MCUI_MENU_LABEL_MAX];
    ui_menu_item_t items[M_COUNT];
    int at = 0;

    for (;;) {
        int pick;

        if (baud_now() > 0)
            snprintf(labels[M_SPEED], MCUI_MENU_LABEL_MAX, "J  Serial speed: %u BAUD",
                     (unsigned)baud_now());
        else
            snprintf(labels[M_SPEED], MCUI_MENU_LABEL_MAX, "J  Serial speed: OFF");
        snprintf(labels[M_POLARITY], MCUI_MENU_LABEL_MAX, "   Serial polarity: Q %s",
                 card.serial.inverted ? "INVERTED" : "DIRECT");
        snprintf(labels[M_ROMS], MCUI_MENU_LABEL_MAX, "O  ROM images (SD card)");
        snprintf(labels[M_MAP], MCUI_MENU_LABEL_MAX, "U  Memory map");
        snprintf(labels[M_CLEAR], MCUI_MENU_LABEL_MAX, "C  Clear the terminal");
        snprintf(labels[M_KEYS], MCUI_MENU_LABEL_MAX, "K  Keys: %s  (F4)",
                 typing ? "TERMINAL" : "PANEL");
        snprintf(labels[M_POWER], MCUI_MENU_LABEL_MAX, "N  Power on again: clear the RAM");
        snprintf(labels[M_TITLE], MCUI_MENU_LABEL_MAX, "T  Title screen");
        items[M_SPEED].text = labels[M_SPEED];       items[M_SPEED].key = 'j';
        items[M_POLARITY].text = labels[M_POLARITY]; items[M_POLARITY].key = 0;
        items[M_ROMS].text = labels[M_ROMS];         items[M_ROMS].key = 'o';
        items[M_MAP].text = labels[M_MAP];           items[M_MAP].key = 'u';
        items[M_CLEAR].text = labels[M_CLEAR];       items[M_CLEAR].key = 'c';
        items[M_KEYS].text = labels[M_KEYS];         items[M_KEYS].key = 'k';
        items[M_POWER].text = labels[M_POWER];       items[M_POWER].key = 'n';
        items[M_TITLE].text = labels[M_TITLE];       items[M_TITLE].key = 't';

        pick = ui_menu(MCUI_MENU_TITLE, items, M_COUNT, MCUI_MENU_HINT, at);
        if (pick < 0)
            return false;
        at = pick;
        switch (pick) {
        case M_SPEED:
            baud_step = (baud_step + 1) % SERIAL_BAUD_STEPS;
            serial_set_speed(&card.serial, baud_now(), MCARD_CYCLES_PER_SEC);
            break;
        case M_POLARITY:
            card.serial.inverted = !card.serial.inverted;
            break;
        case M_ROMS:
            filemenu_rom_images(&target);
            break;
        case M_MAP:
            memview_run(&card.bus);
            break;
        case M_CLEAR:
            terminal_clear();
            return false;
        case M_KEYS:
            typing = !typing;
            return false;
        case M_POWER:
            mcard_init(&card, MCARD_MONITOR, MCARD_MONITOR_SIZE);
            mcard_program_target(&card, &target);
            baud_step = 1;
            terminal_clear();
            set_message("POWERED ON: CLR DOWN. FLIP CLR UP TO RUN");
            return false;
        default:
            return true;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Running                                                             */
/* ------------------------------------------------------------------ */
/* Author: Thomas Dzubin */
void mcardui_run(void)
{
    uint32_t last_us, key_poll_ms, top_ms = 0, panel_ms = 0;
    uint64_t credit = 0;

    if (!powered) {
        mcard_init(&card, MCARD_MONITOR, MCARD_MONITOR_SIZE);
        mcard_program_target(&card, &target);
        terminal_clear();
        set_message("CLR IS DOWN: FLIP C TO RUN THE MONITOR");
        powered = true;
    }
    memset(key_down, 0, sizeof key_down);
    screen_setup();
    last_us = plat_now_us();
    key_poll_ms = plat_now_ms() - KEY_POLL_MS;

    for (;;) {
        uint32_t now_ms = plat_now_ms();
        uint32_t now_us, dt, due;
        int c;

        if (now_ms - key_poll_ms >= KEY_POLL_MS) {
            uint16_t e;

            key_poll_ms = now_ms;
            while ((e = ui_key_event()) != 0) {
                if (handle_key(e)) {
                    if (menu())
                        return;
                    memset(key_down, 0, sizeof key_down);   /* the menu took the releases */
                    screen_setup();
                    last_us = plat_now_us();
                    credit = 0;
                }
            }
        }

        /* as many machine cycles as real time calls for */
        now_us = plat_now_us();
        dt = now_us - last_us;
        last_us = now_us;
        if (dt > MAX_CATCHUP_US)
            dt = MAX_CATCHUP_US;
        credit += (uint64_t)dt * mcard_machine.clock_hz;
        due = (uint32_t)(credit / 1000000u);
        credit -= (uint64_t)due * 1000000u;
        if (due > 0 && mcard_run(&card, due) < due)
            credit = 0;                 /* the card was idle: no catching up */

        while ((c = serial_receive(&card.serial)) >= 0)
            terminal_put((uint8_t)c);
        if (now_ms - top_ms >= MCUI_TERM_MS) {
            top_ms = now_ms;
            terminal_draw();
        }
        if (now_ms - panel_ms >= MCUI_REFRESH_MS) {
            panel_ms = now_ms;
            panel_update();
        }
        plat_sleep_ms(POLL_MS);
    }
}
