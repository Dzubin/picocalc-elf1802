/*
 * helpscreen_const.h - constants of the help screens: the text of every line.
 *
 * Author: Thomas Dzubin
 */
#ifndef HELPSCREEN_CONST_H
#define HELPSCREEN_CONST_H

/* ------------------------------------------------------------------ */
/*  The Elf's help screen (F1, H or ?): every control key, one line      */
/*  each. A line that starts with a hash is a heading (the hash is not   */
/*  shown). The rows are 1 to 29 and the prompt is on row 30.            */
/* ------------------------------------------------------------------ */
#define HELP_ROW_FIRST      1
#define HELP_ROW_PROMPT     30
#define HELP_LINES { \
    "#ELF1802 KEYS", \
    "0-9 A-F        the hex keypad", \
    "I or ENTER     IN button", \
    "R  L  M        RUN, LOAD, memory protect", \
    "TAB            panel / details screen", \
    "V              picture / VDU on top", \
    "F1, H or ?     this help", \
    "F4             ASCII keyboard on / off", \
    "F5             ESC to the program", \
    "ESC            MENU: everything else", \
    "~              BOOTSEL (USB drive mode)", \
    "B (title)      boot program again", \
    "#MENU (ESC)", \
    "UP/DOWN, ENTER, or the row's letter:", \
    "save, open, new, listing, editor,", \
    "debugger, help, memory protect,", \
    "keyboard, sound, picture/VDU, 1861,", \
    "ROM images, title screen", \
    "#ASCII KEYBOARD (F4)", \
    "every key goes to the program, even", \
    "R L M I V H; F4 returns; F1 help", \
    "#SWITCHES (RUN / LOAD, down = off)", \
    "both down      RESET", \
    "LOAD up        LOAD: type 2 digits, IN", \
    "RUN up         RUN from address 0000", \
    "both up        PAUSE", \
    "#DEBUGGER", \
    "S step  G go  H halt  P breakpoint", \
    "arrows scroll W follow T edit ESC leave" }

/* ------------------------------------------------------------------ */
/*  The editor's help screen (F1 in the editor).                         */
/* ------------------------------------------------------------------ */
#define HELP_EDITOR_LINES { \
    "#EDITOR KEYS", \
    "arrows HOME END PGUP PGDN   move", \
    "ENTER          new line, same indent", \
    "BACKSPACE DEL  delete", \
    "TAB            next tab stop", \
    "F1             this help", \
    "ESC            the menu", \
    "#THE MENU (UP/DOWN, ENTER, or letter)", \
    "P  put the program in the RAM", \
    "G  go: assemble and run it", \
    "   debug: assemble, open debugger", \
    "   listing and labels (TAB swaps)", \
    "   next error", \
    "S  save the source   O  open one", \
    "N  new text", \
    "   disassemble the RAM into text", \
    "X  leave the editor", \
    "#ASSEMBLY", \
    "label: OP operand ; comment", \
    "hex numbers, #12 decimal, 'A' letter", \
    "ORG DB DW DS EQU END" }

#endif /* HELPSCREEN_CONST_H */
