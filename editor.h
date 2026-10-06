/*
 * editor.h - a small text editor and assembler screen: type 1802 assembly
 * (asm_core.h says how it is written), assemble it straight into the Elf's RAM,
 * look at a listing and the labels, load and save the source on the SD card,
 * and turn the Elf's RAM back into source text.
 *
 * It runs on the PicoCalc, not in the emulated Elf, and keeps its text between
 * visits. It draws the whole screen, as the debugger does; the caller redraws
 * its own screen afterwards and acts on the result.
 *
 * Keys in the text: the arrow keys, HOME, END, PAGE UP and PAGE DOWN move, ENTER
 * splits a line (the new line starts with the same indent), BACKSPACE and DEL
 * delete, TAB goes to the next tab stop. ESC opens the command menu, and F1 shows
 * the editor's help.
 *
 * Author: Thomas Dzubin
 */
#ifndef EDITOR_H
#define EDITOR_H

#include <stdbool.h>

#include "editor_const.h"
#include "target.h"

typedef struct {
    bool loaded;                    /* an assembly was put in the RAM of the
                                       Elf: put the machine in RESET, ready to
                                       run, and show the picture              */
    int  then;                      /* EDITOR_THEN_*: with loaded, what to do next:
                                       run the program, or open the debugger    */
    char note[EDITOR_NOTE_MAX + 1]; /* a short note to show, or empty         */
} editor_result_t;

/* Replace the text with source (a string, as the program in memory at power-on
 * is spelled) and name it; the text counts as unchanged. */
void editor_preload(const char *source, const char *name);

/* Turn the RAM of m into source text, with a label (L0123) on every address
 * that a branch or jump goes to, and put it in the editor in place of the text
 * there. name is the file the RAM was loaded from (its extension and anything
 * past eight letters and digits is dropped); it is what a save will be called.
 * The text counts as unchanged: the RAM can always make it again. */
void editor_load_ram(const program_target_t *t, const char *name);

/* Open an assembly source file for the Elf's menu: read it into the text (in place
 * of the text there), assemble it and, if it assembles, load it into the RAM of m,
 * ready to run (the result has loaded set and a note). Returns EDITOR_OPEN_OK;
 * EDITOR_OPEN_ERRORS when the file is read but does not assemble (or is too big for the
 * text), in which case editor_run() next shows it with the first error; or
 * EDITOR_OPEN_FAILED when the file could not be read (the result has a note). */
int editor_open_source(const program_target_t *t, const char *name, editor_result_t *result);

/* Run the editor until the user leaves it. The machine m is where an assembly
 * is loaded and what "disassemble" reads. */
void editor_run(const program_target_t *t, editor_result_t *result);

#endif /* EDITOR_H */
