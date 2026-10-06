/*
 * elfpanel.h - the Elf II front panel on the PicoCalc's screen: the title, the
 * two seven-segment hex displays, the Q LED, the 4 x 4 hex keypad, the RUN, LOAD
 * and M/P toggles, the IN button, and the tag under the Q LED that reads ASCII
 * while the keys go to the ASCII keyboard (with F4=OFF, how to leave that mode) and
 * hints at F4 while a program waits
 * for it. The panel knows nothing of the machine: it is told what to show, and
 * draws only what changed since the last time.
 *
 * Author: Thomas Dzubin
 */
#ifndef ELFPANEL_H
#define ELFPANEL_H

#include <stdbool.h>
#include <stdint.h>

/* What the panel shows. */
typedef struct {
    uint32_t lit_keys;      /* the hex keys held now, a bit for each digit 0 to F  */
    bool     in_down;       /* the IN button is held                               */
    bool     sw_run;        /* the RUN, LOAD and memory protect switches are up    */
    bool     sw_load;
    bool     sw_mp;
    bool     ascii_mode;    /* the keys go to the ASCII keyboard                   */
    uint32_t kbd_polls;     /* how many times a program has asked for it so far    */
    uint32_t now_ms;        /* the time, for the pulse of the hint                 */
    int      display;       /* the byte on the two hex displays                    */
    bool     q;             /* the Q line                                          */
} elfpanel_state_t;

/* Forget what is on the screen, so the next update draws it all. */
void elfpanel_forget(void);

/* The parts that never change: the title, the names of the switches, "MENU:ESC". */
void elfpanel_draw_static(void);

/* Bring the parts that move up to date (everything when force is set). */
void elfpanel_update(const elfpanel_state_t *s, bool force);

/* Drawing a hex digit as seven segments, and the Q LED, which the text
 * screen of details uses too. */
void elfpanel_draw_hex_digit(int x, int y, unsigned value);
void elfpanel_draw_q_led(int x, int y, int size, int border, bool on);

#endif /* ELFPANEL_H */
