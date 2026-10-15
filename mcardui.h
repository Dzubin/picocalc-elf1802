/*
 * mcardui.h - the 1802 Membership Card on the PicoCalc's screen: the serial
 * terminal in the top of the screen, the front panel (data LEDs, hex display,
 * data switches, the IN button, WAIT, CLR and READ/WRITE) under it, and a small
 * menu. The card keeps its state between visits, like the Elf does.
 *
 * Author: Thomas Dzubin
 */
#ifndef MCARDUI_H
#define MCARDUI_H

/* Run the card until the user chooses the title screen in its menu. */
void mcardui_run(void);

#endif /* MCARDUI_H */
