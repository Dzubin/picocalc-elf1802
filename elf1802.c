/*
 *  Elf1802
 *  ===========================================
 *
 *  A COSMAC Elf on the ClockworkPi PicoCalc: an RCA CDP1802 microprocessor
 *  with 16K of RAM, a hexadecimal keypad, two hex displays, the Q LED, and a
 *  CDP1861 "Pixie" video chip whose picture fills the top of the screen. The
 *  machine is emulated to the machine cycle (see cpu1802.c, cdp1861.c and
 *  elf.c, which are plain portable C), so programs written for the real Elf
 *  in the 1970s and 1980s, timing loops and video tricks included, can run.
 *
 *  This file is the PicoCalc side of the Elf itself: running the machine in
 *  step with real time, the Elf II front panel and the details screen, the
 *  keys, and the sound. What fills the top of the screen has its own files:
 *  picture.c (the 1861's picture), vdu.c (the VDU) and regview.c (the registers,
 *  for an Elf with no 1861). So do the screens that are not the Elf: title.c
 *  (the title screen), filemenu.c (the menu, saving and opening programs),
 *  helpscreen.c (the help screen) and debugger.c (the debugger view), with the
 *  text and keyboard helpers they share in ui.c.
 *  The screen and the keyboard come from the vendored picocalc-text-starter
 *  drivers by Blair Leduc (picocalc-text-starter-main/):
 *      - lcd_init(), lcd_putstr(), lcd_solid_rectangle(), lcd_blit()
 *      - sb_init(), sb_read_keyboard() (through ui.c)
 *  and the sound goes out through platform.h (plat_audio_*), as samples of the
 *  Q line made by the board (qaudio.c).
 *
 *  Controls on the Elf screen (the keypad screen, the primary one)
 *      0-9 A-F ........ the hex keypad (the last two digits typed are kept)
 *      I or ENTER ..... the IN button (held down it pulls EF4 low)
 *      R .............. flip the RUN switch
 *      L .............. flip the LOAD switch
 *      M .............. flip the memory protect switch
 *      V .............. show the VDU or the 1861 picture (it follows the
 *                       program by itself: the picture when the 1861 turns on,
 *                       the VDU when the VDU's memory is written)
 *      TAB ............ the screen under the picture: the Elf II front panel
 *                       (the default: keys, switches and displays that move
 *                       as you press the PicoCalc keys) or the details
 *                       (registers, speed, key help)
 *      F1, H or ? ..... the help screen, which lists every control key
 *      F4 ............. the ASCII keyboard (port 7, flag EF3) on and off, the
 *                       same key both ways: every key, R L M I V H too, is typed
 *                       to the program, and the tag under the Q LED says F4=OFF
 *      F5 ............. sends ESC (1B) to the program from the ASCII keyboard
 *      ESC ............ the menu, the way to everything else: save, open, new,
 *                       listing file, the editor and assembler, the debugger,
 *                       help, memory protect, keyboard, sound, picture or VDU,
 *                       the 1861, ROM images, the title screen
 *      ~ (SHIFT+`) .... reboot into BOOTSEL (USB drive) mode, from anywhere
 *
 *  Only the unshifted function keys F1 to F5 are used (F6 to F10 are Shift+F1
 *  to F5 on the PicoCalc). In the debugger S steps, G goes, H halts, P sets the
 *  breakpoint, W follows the PC, T opens the editor and ESC leaves it.
 *
 *  RUN and LOAD work as on the Netronics Elf II (see elf.h): both down is
 *  RESET, LOAD up alone lets you type bytes into memory, RUN up alone runs
 *  the program from address 0000.
 *
 *  Headers keep this file to code only: elf1802_const.h holds the constants
 *  of this file (elf_const.h those of the emulated machine, and each screen
 *  module its own _const.h), and platform.h is the whole interface to the rest
 *  of the machine (clocks, sleep, sound, saved programs, BOOTSEL, exit to the
 *  UF2 Loader), implemented in platform_pico.c for the PicoCalc and in
 *  desktop/platform_desktop.c for Windows and Linux.
 *
 *  Author: Thomas Dzubin
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "lcd.h"          /* WIDTH, HEIGHT, GLYPH_HEIGHT, RGB(), lcd_*()      */
#include "southbridge.h"  /* sb_init(), sb_read_keyboard()                    */
#include "keyboard.h"     /* KEY_* codes, KEY_STATE_* values                  */

#include "platform.h"     /* clocks, sleep, sound, files, BOOTSEL, loader exit */
#include "elf.h"          /* the emulated computer                            */
#include "boot_program.h"  /* the program in memory at power-on (RANDTONE)     */
#include "boot_source.h"   /* its assembly source, for the editor              */
#include "debugger.h"     /* the debugger view                               */
#include "editor.h"       /* the text editor and assembler                    */
#include "elfpanel.h"     /* the Elf II front panel                           */
#include "elf1802_const.h" /* the constants of this file and the screens' colours */
#include "filemenu.h"     /* the menu: save, open, clear, leave               */
#include "helpscreen.h"   /* the help screen                                  */
#include "picture.h"      /* the 1861's picture on the screen                 */
#include "regview.h"      /* the register read-out when there is no 1861      */
#include "rom.h"          /* the ROM images that are fitted                   */
#include "title.h"        /* the title screen                                 */
#include "ui.h"           /* text, keyboard, the ways out                     */
#include "vdu.h"          /* the VDU on the screen                            */

/* the board's samples are played at the rate the platform plays them */
_Static_assert(QAUDIO_RATE == PLAT_AUDIO_RATE, "sample rates differ");

/* ===================================================================== */
/*  The computer and what the screen shows of it                          */
/* ===================================================================== */
static elf_t elf;
static program_target_t target;                 /* its memory, for the editor and the menu */
static const machine_t *const emu = &elf_machine;   /* how to run it, and how fast */

static bool sw_run, sw_load, sw_mp;       /* the front-panel switches        */

static bool     view_vdu;                 /* the top of the screen shows the VDU */
static uint32_t vdu_writes_seen;
static bool     ascii_mode;               /* keys go to the ASCII keyboard   */
static bool     key_down[256];            /* keys held, by lowercase code    */
static uint32_t key_press_ms[256];        /* when each was last pressed      */
static bool     sound_on = SOUND_ON_AT_START;
static bool     sound_playing;            /* samples have been handed over   */
static uint32_t sound_last_ms;            /* when samples last came          */

static bool     view_details;             /* the details screen, not the panel */

static int      shown_display;            /* hex display value, -1 = unknown */
static int      shown_q;                  /* Q LED state, -1 = unknown       */

static char     message[TCOLS + 1];       /* the note on ROW_MESSAGE         */
static uint32_t message_until_ms;

#define KEYRES_NONE     0
#define KEYRES_MENU     1
#define KEYRES_HELP     2
#define KEYRES_EDIT     3       /* the debugger asked for the editor */

/* ===================================================================== */
/*  Forward declarations (so main() can sit at the top)                   */
/* ===================================================================== */
static void run_computer(void);
static void power_on(void);
static void screen_setup(void);
static void apply_switches(void);
static int  handle_key(uint16_t e);
static int  hex_value(uint8_t code);

static void set_view(bool vdu);
static void help_rows(void);
static void run_editor(char *note, size_t note_size);
static void do_menu_action(int action, char *note, size_t note_size);
static void ascii_key(uint8_t code);
static void function_key(uint8_t code, uint8_t state);
static void draw_hex_digit(int x, int y, unsigned value);
static void draw_q_led(bool on);
static void lower_setup(void);
static void panel_update(bool force);
static void update_panel(bool force);
static void update_registers(uint32_t speed_pct, bool speed_known, uint32_t busy_pct);
static void set_message(const char *text);

static void sound_update(void);
static void sound_stop(void);

/* ===================================================================== */
/*  main: set up the hardware once, then splash -> the Elf -> splash      */
/* ===================================================================== */
/* Author: Thomas Dzubin */
int main(void)
{
    plat_init();

    sb_init();                /* keyboard / south-bridge I2C (no bg poll) */
    lcd_init();               /* ST7365P LCD */
    lcd_enable_cursor(false); /* no blinking text cursor */
    lcd_set_background(COL_BG);
    plat_audio_init();        /* the speaker, played from the Q line's samples */

    rom_config_load();        /* the ROM images that were fitted last time */
    elf_init(&elf);           /* power on once; leaving the Elf keeps it */
    elf_program_target(&elf, &target);
    elf_load_image(&elf, BOOT_PROGRAM, BOOT_PROGRAM_SIZE);  /* ready to run: press R */
    editor_preload(BOOT_SOURCE, BOOT_SOURCE_NAME);
    debugger_attach(&elf);

    for (;;) {
        title_screen();

        uint8_t key = ui_wait_any_key();
        if (key == KEY_ESC || key == 'q' || key == 'Q')
            ui_go_loader();      /* does not return */
        if (key == KEY_BOOT_PROGRAM || key == KEY_BOOT_PROGRAM - 'a' + 'A')
            power_on();       /* B: the boot program again, the machine afresh */
        run_computer();       /* returns when the menu says to leave */
    }
}

/* Switch the Elf on afresh: everything cleared, the RANDTONE program in memory
 * and the machine in RESET, ready to run (press R). Done by the B key on the
 * title screen, so that program can be brought back (ESC, X, B) after others
 * have been opened. */
static void power_on(void)
{
    elf_init(&elf);
    elf_load_image(&elf, BOOT_PROGRAM, BOOT_PROGRAM_SIZE);
    editor_preload(BOOT_SOURCE, BOOT_SOURCE_NAME);
    sw_run = sw_load = sw_mp = false;
    apply_switches();
    ascii_mode = false;
    view_vdu = false;
    debugger_reset();
    message[0] = '\0';
    sound_stop();
}

/* ===================================================================== */
/*  Keys                                                                  */
/* ===================================================================== */
/* the value 0 to 15 of a hex digit key, or -1 for any other key */
static int hex_value(uint8_t code)
{
    if (code >= '0' && code <= '9')
        return code - '0';
    code = ui_lower(code);
    if (code >= 'a' && code <= 'f')
        return code - 'a' + 10;
    return -1;
}

/* ===================================================================== */
/*  The Elf: running the machine and keeping the screen up to date        */
/* ===================================================================== */
/* Author: Thomas Dzubin */
static void run_computer(void)
{
    uint32_t last_us = plat_now_us();
    uint64_t credit = 0;                /* microseconds * cycles per second */
    uint32_t speed_start_ms = plat_now_ms();
    uint64_t speed_cycles = 0;
    uint32_t speed_pct = 0;
    uint32_t busy_us = 0;               /* time spent running the machine */
    uint32_t busy_pct = 0;
    bool speed_known = false;
    uint32_t panel_ms = 0;
    uint32_t vdu_ms = 0;
    uint32_t top_ms = 0;
    uint32_t key_poll_ms = plat_now_ms() - KEY_POLL_MS;     /* read at once */
    bool video_was_on = elf.video.on;

    vdu_writes_seen = elf.vdu_writes;
    screen_setup();

    for (;;) {
        /* keys: each read of the keyboard chip is a wait of several ms, so it
         * is read a few dozen times a second, all the waiting keys at once */
        if (plat_now_ms() - key_poll_ms >= KEY_POLL_MS) {
            uint16_t e;

            key_poll_ms = plat_now_ms();
            while ((e = ui_key_event()) != 0) {
                int result = handle_key(e);

                if (result == KEYRES_HELP) {
                    sound_stop();
                    help_screen();
                    memset(key_down, 0, sizeof key_down);   /* it took the releases */
                    screen_setup();
                    last_us = plat_now_us();
                    credit = 0;
                } else if (result == KEYRES_EDIT) {     /* T in the debugger */
                    char note[EDITOR_NOTE_MAX + 1];

                    run_editor(note, sizeof note);
                    memset(key_down, 0, sizeof key_down);
                    screen_setup();
                    if (note[0])
                        set_message(note);
                    last_us = plat_now_us();
                    credit = 0;
                } else if (result == KEYRES_MENU) {
                    filemenu_result_t menu;
                    filemenu_machine_t state;

                    state.mp_on = sw_mp;
                    state.sound_on = sound_on;
                    state.ascii_mode = ascii_mode;
                    state.vdu_shown = view_vdu;
                    sound_stop();
                    filemenu_run(&target, &state, &menu);
                    if (menu.leave)
                        return;
                    if (menu.opened)            /* a file: its source is in the editor now */
                        editor_load_ram(&target, menu.name);
                    if (menu.source[0]) {       /* an .ASM file: read, assembled and loaded */
                        editor_result_t src;
                        int r = editor_open_source(&target, menu.source, &src);

                        if (r == EDITOR_OPEN_OK)
                            menu.opened = true;
                        else if (r == EDITOR_OPEN_ERRORS)
                            menu.edit = true;   /* the editor shows what is wrong */
                        snprintf(menu.note, sizeof menu.note, "%s", src.note);
                    }
                    if (menu.opened) {          /* the machine is in RESET, ready to run */
                        sw_run = sw_load = false;
                        apply_switches();
                    }
                    if (menu.opened || menu.cleared || menu.video_changed)
                        view_vdu = false;       /* back to the picture (or the registers) */
                    if (menu.edit) {            /* the editor; an assembly loads the RAM */
                        char note[EDITOR_NOTE_MAX + 1];

                        run_editor(note, sizeof note);
                        if (note[0])
                            snprintf(menu.note, sizeof menu.note, "%s", note);
                    }
                    do_menu_action(menu.action, menu.note, sizeof menu.note);
                    memset(key_down, 0, sizeof key_down);   /* the menu took the releases */
                    screen_setup();
                    if (menu.note[0])
                        set_message(menu.note);
                    last_us = plat_now_us();
                    credit = 0;
                }
            }
            key_poll_ms = plat_now_ms();
        }

        /* emulate as many machine cycles as real time calls for (none while
         * the debugger has the Elf stopped) */
        uint32_t now_us = plat_now_us();
        uint32_t dt = now_us - last_us;
        last_us = now_us;
        if (dt > MAX_CATCHUP_US)
            dt = MAX_CATCHUP_US;
        if (debugger_halted()) {
            credit = 0;
        } else {
            credit += (uint64_t)dt * emu->clock_hz;
            uint32_t due = (uint32_t)(credit / 1000000u);
            credit -= (uint64_t)due * 1000000u;
            uint32_t done;
            uint32_t started_us = plat_now_us();

            int32_t breakpoint = debugger_break_address();

            if (breakpoint >= 0 && emu->run_to) {
                bool hit;

                done = emu->run_to(&elf, due, (uint16_t)breakpoint, &hit);
                if (hit)
                    debugger_break_hit();
            } else {
                done = emu->run(&elf, due);
            }
            busy_us += plat_now_us() - started_us;
            if (done < due)
                credit = 0;             /* the machine was idle: no catching up */
            speed_cycles += done;
        }

        /* the Q line as sound, the picture and the panel */
        uint32_t now_ms = plat_now_ms();
        sound_update();

        /* the top of the screen follows the program: the 1861 picture when the
         * chip comes on, the VDU when its memory is written to */
        bool video_came_on = elf.video.on && !video_was_on;
        bool vdu_written = elf.vdu_writes != vdu_writes_seen;

        video_was_on = elf.video.on;
        vdu_writes_seen = elf.vdu_writes;
        if (debugger_active()) {
            if (debugger_dirty() || now_ms - panel_ms >= PANEL_REFRESH_MS) {
                panel_ms = now_ms;
                debugger_draw();
            }
        } else {
            if (video_came_on)
                set_view(false);
            if (vdu_written && !elf.video.on && !view_vdu)
                set_view(true);

            if (view_vdu) {
                if (now_ms - vdu_ms >= VDU_DRAW_MS) {
                    vdu_ms = now_ms;
                    vdu_draw(&elf);
                }
            } else if (elf.video_installed) {
                picture_update(&elf);
            } else if (now_ms - top_ms >= PANEL_REFRESH_MS) {
                top_ms = now_ms;                /* no 1861: the registers instead */
                regview_update(&elf);
            }
        }

        if (now_ms - speed_start_ms >= SPEED_WINDOW_MS) {
            uint64_t full = (uint64_t)emu->clock_hz * (now_ms - speed_start_ms) / 1000u;
            speed_pct = full ? (uint32_t)(speed_cycles * 100u / full) : 0;
            speed_known = elf.cpu.mode == CPU_MODE_RUN;
            busy_pct = busy_us / (SPEED_WINDOW_MS * 10u);   /* of the window, in percent */
            busy_us = 0;
            speed_cycles = 0;
            speed_start_ms = now_ms;
        }
        if (!debugger_active()) {
            if (!view_details) {
                panel_update(false);
            } else {
                if (now_ms - panel_ms >= PANEL_REFRESH_MS) {
                    panel_ms = now_ms;
                    update_registers(speed_pct, speed_known, busy_pct);
                }
                update_panel(false);
            }
        }

        plat_sleep_ms(POLL_MS);
    }
}

/* Draw the parts of the Elf screen that never change, and forget what the
 * moving parts showed so that they are drawn afresh. */
static void screen_setup(void)
{
    lcd_clear_screen();
    ui_forget_rows(0);
    picture_forget(&elf, false);
    vdu_forget();
    regview_forget();

    if (debugger_active()) {
        debugger_draw();
        return;
    }
    lower_setup();
}

/* Draw the screen under the picture afresh: the Elf II panel, or the details
 * (the title, the displays and switches as text, the registers, the key
 * help). Used when the screen is set up and when TAB switches between them.
 *
 * Author: Thomas Dzubin */
static void lower_setup(void)
{
    lcd_solid_rectangle(COL_BG, 0, TOP_AREA_HEIGHT, WIDTH, HEIGHT - TOP_AREA_HEIGHT);
    ui_forget_rows(ROW_MODE);
    shown_display = -1;
    shown_q = -1;
    elfpanel_forget();

    if (!view_details) {
        elfpanel_draw_static();
        panel_update(true);
        return;
    }
    ui_put_at(0, ROW_MODE, "ELF1802  RCA 1802 + 1861 + VDU, 16K RAM", COL_CYAN);
    ui_put_at(DISPLAY_LABEL_COL, DISPLAY_LABEL_ROW, "DATA", COL_WHITE);
    ui_put_at(Q_LABEL_COL, Q_LABEL_ROW, "Q", COL_WHITE);
    help_rows();
    update_panel(true);
    if (message[0])
        ui_panel_row(ROW_MESSAGE, 0, message, COL_YELLOW);
    update_registers(0, false, 0);
}

static void apply_switches(void)
{
    elf_set_switches(&elf, sw_run, sw_load, sw_mp);
}

/* The four help lines at the bottom, for the keyboard mode in use. */
static void help_rows(void)
{
    if (debugger_active() || !view_details)
        return;                         /* those rows belong to another screen */
    if (ascii_mode) {
        ui_panel_row(ROW_HELP_1, 0, "ASCII KEYBOARD: keys go to port 7 (EF3)", COL_YELLOW);
        ui_panel_row(ROW_HELP_2, 0, "F4: hex keypad again     F5: sends ESC", COL_WHITE);
        ui_panel_row(ROW_HELP_3, 0, "R L M I V H are typed to the program", COL_WHITE);
    } else {
        ui_panel_row(ROW_HELP_1, 0, "0-9 A-F: keypad    I / ENTER: IN button", COL_WHITE);
        ui_panel_row(ROW_HELP_2, 0, "R RUN, L LOAD, M memory protect", COL_WHITE);
        ui_panel_row(ROW_HELP_3, 0, "TAB: panel  F4: ASCII kbd  V: VDU/pic", COL_WHITE);
    }
    ui_panel_row(ROW_HELP_4, 0, "ESC: menu (everything else)   F1: help", COL_WHITE);
}

/* A key on the ASCII keyboard: the printable keys, ENTER (as CR) and
 * BACKSPACE. */
static void ascii_key(uint8_t code)
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
        elf_ascii_key(&elf, out);
}

/* The function keys: F4 switches the ASCII keyboard on and off (the one key for
 * both), and F5 sends ESC to the program (F1, the help, is handled by the caller).
 *
 * Author: Thomas Dzubin */
static void function_key(uint8_t code, uint8_t state)
{
    if (state != KEY_STATE_PRESSED)
        return;
    if (code == FKEY_ESC) {
        elf_ascii_key(&elf, ASCII_ESCAPE);
    } else if (code == FKEY_ASCII) {
        ascii_mode = !ascii_mode;
        help_rows();
        set_message(ascii_mode ? "ASCII KEYBOARD: F4 FOR THE KEYPAD" : "HEX KEYPAD");
    }
}

/* Run the editor and act on how it was left. An assembly has loaded the RAM, so
 * the machine is in RESET, ready to run; the editor may also ask for the program
 * to start (RUN up) or for the debugger on it, stopped at its first instruction.
 * note gets the editor's note, or is empty.
 *
 * Author: Thomas Dzubin */
static void run_editor(char *note, size_t note_size)
{
    editor_result_t edited;

    sound_stop();
    editor_run(&target, &edited);
    note[0] = '\0';
    if (!edited.loaded)
        return;
    snprintf(note, note_size, "%s", edited.note);
    sw_run = edited.then != EDITOR_THEN_NONE;       /* RUN up starts the program */
    sw_load = false;
    apply_switches();
    view_vdu = false;                   /* back to the picture (or the registers) */
    if (edited.then == EDITOR_THEN_DEBUG)
        debugger_enter();
    else if (edited.then == EDITOR_THEN_RUN && debugger_active())
        debugger_leave();
}

/* What the menu asked for besides its own screens.
 *
 * Author: Thomas Dzubin */
static void do_menu_action(int action, char *note, size_t note_size)
{
    switch (action) {
    case FILEMENU_ACT_DEBUGGER:
        debugger_enter();
        break;
    case FILEMENU_ACT_HELP:
        help_screen();
        break;
    case FILEMENU_ACT_PROTECT:
        sw_mp = !sw_mp;
        apply_switches();
        snprintf(note, note_size, "MEMORY PROTECT %s", sw_mp ? "ON" : "OFF");
        break;
    case FILEMENU_ACT_KEYBOARD:
        ascii_mode = !ascii_mode;
        snprintf(note, note_size, "%s", ascii_mode ? "ASCII KEYBOARD: F4 FOR THE KEYPAD" : "HEX KEYPAD");
        break;
    case FILEMENU_ACT_SOUND:
        sound_on = !sound_on;
        snprintf(note, note_size, "SOUND %s", sound_on ? "ON" : "OFF");
        break;
    case FILEMENU_ACT_VIEW:
        view_vdu = !view_vdu;           /* the screen is drawn afresh by the caller */
        break;
    default:
        break;
    }
}

/* The keyboard chip sends a held key's auto-repeat as more "pressed" events,
 * with no release between them. The held keys are kept here (by lowercase code,
 * so a letter pressed as a capital and let go as lowercase is the same key) to
 * tell a repeat from a new press. A key that has been down for KEY_STALE_MS
 * with no repeat is counted as let go, because the chip can send its release
 * under a different code (Shift pressed in between) and it would stay "held".
 * (key_down and key_press_ms are declared with the other state at the top.) */

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

/* A repeat is wanted when typing on the ASCII keyboard, and for stepping and
 * scrolling in the debugger; holding a hex key or a switch key must not press
 * it again and again (it would push the digit into the keypad twice, or flip
 * a switch back and forth). */
static bool repeat_wanted(uint8_t code)
{
    if (debugger_active())
        return debugger_wants_repeat(code);
    return ascii_mode && code < 0x80;
}

/* One keyboard event while the Elf is on screen.
 *
 * Author: Thomas Dzubin */
static int handle_key(uint16_t e)
{
    uint8_t code = ui_ev_code(e);
    uint8_t state = ui_ev_state(e);
    uint8_t key = ui_lower(code);
    int digit;

    if (state == KEY_STATE_PRESSED && code == '~')
        ui_go_bootsel();
    if (ui_is_mod_key(code))
        return KEYRES_NONE;
    if (key_repeat(code, state) && !repeat_wanted(code))
        return KEYRES_NONE;

    if (code == FKEY_HELP)
        return state == KEY_STATE_PRESSED ? KEYRES_HELP : KEYRES_NONE;
    if (code == FKEY_ESC || code == FKEY_ASCII) {
        function_key(code, state);
        return KEYRES_NONE;
    }
    if (state == KEY_STATE_PRESSED && code == KEY_SCREEN_TOGGLE) {
        if (!debugger_active()) {
            view_details = !view_details;
            lower_setup();
        }
        return KEYRES_NONE;
    }
    if (debugger_active() && state == KEY_STATE_PRESSED) {
        int used = debugger_key(code);

        if (used == DEBUGGER_KEY_EDIT)
            return KEYRES_EDIT;
        if (used == DEBUGGER_KEY_LEFT)
            screen_setup();             /* back to the Elf's own screen */
        if (used != DEBUGGER_KEY_PASS)
            return KEYRES_NONE;         /* a debugger key (ESC leaves it) */
    }
    /* H and ? open the help, except on the ASCII keyboard, where they are typed
     * (the debugger has taken its H, halt, above) */
    if (!ascii_mode && state == KEY_STATE_PRESSED &&
        (key == KEY_HELP || code == KEY_HELP_QUESTION))
        return KEYRES_HELP;
    if (state == KEY_STATE_PRESSED && code == KEY_ESC)
        return KEYRES_MENU;
    if (ascii_mode) {
        if (state == KEY_STATE_PRESSED)
            ascii_key(code);
        return KEYRES_NONE;
    }

    /* the IN button is down from the press to the release */
    if (key == KEY_IN_BUTTON || code == KEY_ENTER || code == KEY_RETURN) {
        if (state == KEY_STATE_PRESSED)
            elf_in_button(&elf, true);
        else if (state == KEY_STATE_RELEASED)
            elf_in_button(&elf, false);
        return KEYRES_NONE;
    }

    if (state != KEY_STATE_PRESSED)
        return KEYRES_NONE;

    digit = hex_value(code);
    if (digit >= 0) {
        elf_key_hex(&elf, (unsigned)digit);
    } else if (key == KEY_SW_RUN) {
        sw_run = !sw_run;
        apply_switches();
    } else if (key == KEY_SW_LOAD) {
        sw_load = !sw_load;
        apply_switches();
    } else if (key == KEY_SW_MP) {
        sw_mp = !sw_mp;
        apply_switches();
    } else if (key == KEY_VIEW) {
        set_view(!view_vdu);
    }
    return KEYRES_NONE;
}

/* ===================================================================== */
/*  The top of the screen: the picture, the VDU or the registers          */
/* ===================================================================== */
/* Choose what the top of the screen shows (the VDU, or the 1861's picture or the
 * register read-out), and blank it for the change. */
static void set_view(bool vdu)
{
    if (vdu == view_vdu)
        return;
    view_vdu = vdu;
    lcd_solid_rectangle(COL_BG, 0, 0, WIDTH, TOP_AREA_HEIGHT);
    picture_forget(&elf, true);                 /* draw the picture at once */
    vdu_forget();
    regview_forget();
}

/* ===================================================================== */
/*  The front panel: hex displays, Q LED, switches                        */
/* ===================================================================== */
static void draw_hex_digit(int x, int y, unsigned value)
{
    elfpanel_draw_hex_digit(x, y, value);
}

static void draw_q_led(bool on)
{
    elfpanel_draw_q_led(Q_LED_X, Q_LED_Y, Q_LED_SIZE, Q_LED_BORDER, on);
}

/* The hex keys that are held down now (a bit for each digit 0 to F). The
 * keyboard chip's repeats keep a held key's time fresh, and a key with none
 * for KEY_STALE_MS is let go, as in key_repeat(). */
static uint32_t panel_lit_keys(void)
{
    uint32_t now_ms = plat_now_ms();
    uint32_t mask = 0;
    int v;

    if (ascii_mode)
        return 0;                       /* the keys go to the program instead */
    for (v = 0; v < 16; v++) {
        uint8_t id = (uint8_t)(v < 10 ? '0' + v : 'a' + v - 10);

        if (key_down[id] && now_ms - key_press_ms[id] < KEY_STALE_MS)
            mask |= 1u << v;
    }
    return mask;
}

/* Bring the parts of the Elf II panel that move up to date (everything when
 * force is set). */
static void panel_update(bool force)
{
    elfpanel_state_t st;

    st.lit_keys = panel_lit_keys();
    st.in_down = elf.in_button;
    st.sw_run = sw_run;
    st.sw_load = sw_load;
    st.sw_mp = sw_mp;
    st.ascii_mode = ascii_mode;
    st.kbd_polls = elf.kbd_polls;
    st.now_ms = plat_now_ms();
    st.display = elf.display;
    st.q = elf.cpu.q;
    elfpanel_update(&st, force);
}

static const char *mode_name(void)
{
    switch (elf.cpu.mode) {
    case CPU_MODE_LOAD:  return "LOAD";
    case CPU_MODE_RESET: return "RESET";
    case CPU_MODE_PAUSE: return "PAUSE";
    default:             return "RUN";
    }
}

/* The parts that follow the front panel: displays, LED, switches.
 *
 * Author: Thomas Dzubin */
static void update_panel(bool force)
{
    char line[TCOLS + 1];
    bool q = elf.cpu.q;

    if (force || shown_display != elf.display) {
        shown_display = elf.display;
        draw_hex_digit(DISPLAY_X_HIGH, DISPLAY_Y, (unsigned)(elf.display >> 4));
        draw_hex_digit(DISPLAY_X_LOW, DISPLAY_Y, (unsigned)(elf.display & 15));
    }
    if (force || shown_q != (int)q) {
        shown_q = q;
        draw_q_led(q);
    }

    snprintf(line, sizeof line, "RUN  [R] %s", sw_run ? "UP" : "DOWN");
    ui_panel_row(ROW_SW_RUN, PANEL_COL, line, sw_run ? COL_GREEN : COL_WHITE);
    snprintf(line, sizeof line, "LOAD [L] %s", sw_load ? "UP" : "DOWN");
    ui_panel_row(ROW_SW_LOAD, PANEL_COL, line, sw_load ? COL_GREEN : COL_WHITE);
    snprintf(line, sizeof line, "MP   [M] %s", sw_mp ? "ON" : "OFF");
    ui_panel_row(ROW_SW_MP, PANEL_COL, line, sw_mp ? COL_YELLOW : COL_WHITE);
    if (ascii_mode)
        snprintf(line, sizeof line, "ASCII  F4=KEYPAD");
    else
        snprintf(line, sizeof line, "KEYS %02X  IN %s", elf.key_latch,
                 elf.in_button ? "DOWN" : "UP");
    ui_panel_row(ROW_IN_KEYPAD, PANEL_COL, line, COL_WHITE);
    snprintf(line, sizeof line, "MODE %s", mode_name());
    ui_panel_row(ROW_PANEL_MODE, PANEL_COL, line, COL_CYAN);

    if (message[0] && plat_now_ms() >= message_until_ms) {
        message[0] = '\0';
        ui_panel_row(ROW_MESSAGE, 0, "", COL_YELLOW);
    }
}

/* The CPU's registers and the emulation speed (100% is the real Elf's), with
 * the share of the time that running the machine took (EMU: what is left is
 * for the screen, the keyboard and the sound).
 *
 * Author: Thomas Dzubin */
static void update_registers(uint32_t speed_pct, bool speed_known, uint32_t busy_pct)
{
    const cpu1802_t *c = &elf.cpu;
    char line[TCOLS + 1];
    char speed[5];

    snprintf(line, sizeof line, "P:%X X:%X D:%02X DF:%d Q:%d IE:%d",
             c->p, c->x, c->d, c->df ? 1 : 0, c->q ? 1 : 0, c->ie ? 1 : 0);
    ui_panel_row(ROW_REG_1, 0, line, COL_GREEN);
    snprintf(line, sizeof line, "R0:%04X R1:%04X R2:%04X R3:%04X",
             c->r[0], c->r[1], c->r[2], c->r[3]);
    ui_panel_row(ROW_REG_2, 0, line, COL_GREEN);

    if (speed_known)
        snprintf(speed, sizeof speed, "%3u%%",
                 (unsigned)(speed_pct > 999 ? 999 : speed_pct));
    else
        snprintf(speed, sizeof speed, " ---");
    snprintf(line, sizeof line, "PC:%04X I:%X%X  EMU:%3u%%  SPEED:%s",
             c->r[c->p], c->i, c->n, (unsigned)(busy_pct > 999 ? 999 : busy_pct), speed);
    ui_panel_row(ROW_REG_3, 0, line, COL_GREEN);
}

static void set_message(const char *text)
{
    strncpy(message, text, TCOLS);
    message[TCOLS] = '\0';
    message_until_ms = plat_now_ms() + MESSAGE_MS;
    if (view_details && !debugger_active())     /* the other screens have no room for it */
        ui_panel_row(ROW_MESSAGE, 0, message, COL_YELLOW);
}

/* ===================================================================== */
/*  The Q line as sound                                                   */
/* ===================================================================== */
/* Hand the platform the samples the board has made since the last turn of the
 * main loop. With the sound off, or the Elf stopped by the debugger, they are
 * thrown away. When no samples have come for a while (the Elf is in RESET,
 * PAUSE or an idle LOAD) the speaker is silenced, or it would go on playing the
 * last few milliseconds over and over. */
static void sound_update(void)
{
    int16_t samples[SOUND_CHUNK];
    uint32_t now_ms = plat_now_ms();
    uint32_t n;

    while ((n = qaudio_read(&elf.audio, samples, SOUND_CHUNK)) > 0) {
        sound_last_ms = now_ms;
        if (sound_on && !debugger_halted()) {
            plat_audio_play(samples, n);
            sound_playing = true;
        }
    }
    if (sound_playing && now_ms - sound_last_ms >= SOUND_IDLE_MS)
        sound_stop();
}

static void sound_stop(void)
{
    plat_audio_stop();
    qaudio_flush(&elf.audio);
    sound_playing = false;
}
