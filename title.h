/*
 * title.h - the title screen: the name, the version, the credits and what the
 * keys do (any key starts the Elf, B starts it afresh with the boot program,
 * ESC or Q leaves for the UF2 Loader). It only draws; main() reads the key.
 *
 * Author: Thomas Dzubin
 */
#ifndef TITLE_H
#define TITLE_H

void title_screen(void);

#endif /* TITLE_H */
