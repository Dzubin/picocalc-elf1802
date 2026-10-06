/*
 * helpscreen.h - the help screens: every control key on one screen, until a key
 * is pressed (F1, H or ? opens the Elf's; F1 in the editor opens the editor's).
 *
 * Author: Thomas Dzubin
 */
#ifndef HELPSCREEN_H
#define HELPSCREEN_H

/* Draw the help and wait for a key. The caller redraws its own screen after. */
void help_screen(void);             /* the Elf's keys                        */
void help_screen_editor(void);      /* the editor's keys                     */

#endif /* HELPSCREEN_H */
