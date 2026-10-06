/*
 * filemenu.h - the menu (ESC on the Elf screen), the one way to everything the Elf
 * can do that has no key of its own on the keypad screen. It is an arrow-key menu
 * (ui_menu()) in three groups: FILES (SD CARD) (save the RAM to the SD card, open a .BIN,
 * .HEX or .ASM file, clear the RAM, write a listing file, ROM images), TOOLS (the
 * editor and assembler, the debugger, help) and MACHINE (memory protect, the
 * keyboard, sound, the picture or VDU, the 1861), and last the way back to the title
 * screen. It draws its own screens and waits for keys; what the Elf screen has to
 * do afterwards is reported in the result.
 *
 * Author: Thomas Dzubin
 */
#ifndef FILEMENU_H
#define FILEMENU_H

#include <stdbool.h>
#include <stddef.h>

#include "target.h"
#include "filemenu_const.h"
#include "platform_const.h"

/* What the menu shows of the machine's switches (the menu does not change them;
 * the caller does what the result's action says). */
typedef struct {
    bool mp_on;                     /* the memory protect switch is on           */
    bool sound_on;
    bool ascii_mode;                /* the keys go to the ASCII keyboard         */
    bool vdu_shown;                 /* the top of the screen shows the VDU       */
} filemenu_machine_t;

/* What the caller is to do besides what the flags below say. */
#define FILEMENU_ACT_NONE       0
#define FILEMENU_ACT_DEBUGGER   1   /* open the debugger                         */
#define FILEMENU_ACT_HELP       2   /* show the help screen                      */
#define FILEMENU_ACT_PROTECT    3   /* flip the memory protect switch            */
#define FILEMENU_ACT_KEYBOARD   4   /* the hex keypad <-> the ASCII keyboard     */
#define FILEMENU_ACT_SOUND      5   /* sound on or off                           */
#define FILEMENU_ACT_VIEW       6   /* the VDU <-> the picture (or registers)    */

typedef struct {
    int  action;                    /* FILEMENU_ACT_*                            */
    bool leave;                     /* leave the Elf for the title screen       */
    bool opened;                    /* a program was opened: put the machine in
                                       RESET (both switches down) and show the
                                       1861 picture                             */
    bool cleared;                   /* the RAM was cleared: show the picture     */
    bool video_changed;             /* the 1861 was fitted or taken off the board:
                                       show the picture (or the registers)       */
    bool edit;                      /* run the editor (the caller does)          */
    char source[PLAT_NAME_MAX];     /* an .ASM file was chosen: the caller has the
                                       editor load and assemble it (opened is
                                       not set, the editor says how it went)     */
    char note[FILEMENU_NOTE_MAX + 1];   /* a short note to show (SAVED NAME.BIN,
                                           OPENED NAME.BIN, RAM CLEARED), or empty */
    char name[PLAT_NAME_MAX];       /* the file that was opened (with opened)    */
} filemenu_result_t;

/* Show the menu until the user leaves it, doing what was asked of the RAM of t.
 * m says how the machine's switches are now. The caller redraws its screen
 * afterwards and acts on the result. */
void filemenu_run(const program_target_t *t, const filemenu_machine_t *m,
                  filemenu_result_t *result);

/* The file chooser and the name entry of the menu, for other screens (the
 * editor) to use. filemenu_pick_file() lists the files of a kind (PLAT_KIND_*
 * in platform_const.h) and returns true with the chosen file's name (up to
 * PLAT_NAME_MAX characters with the NUL); each kind's list starts on the file it
 * was left on. filemenu_ask_name() asks for a name of letters and digits, which
 * starts as the text already in name (SAVE_NAME_MAX + 1 characters at least)
 * and is shown with the extension ext; true when the user pressed ENTER.
 * filemenu_error_text() describes a PLAT_FILE_* result. */
bool filemenu_pick_file(const char *title, int kind, char *name);
bool filemenu_ask_name(const char *title, const char *ext, char *name, size_t size);
const char *filemenu_error_text(int rc);

#endif /* FILEMENU_H */
