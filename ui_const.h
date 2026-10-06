/*
 * ui_const.h - constants of the shared screen and keyboard helpers (ui.c).
 *
 * Author: Thomas Dzubin
 */
#ifndef UI_CONST_H
#define UI_CONST_H

/* The title screen's prompt and exit lines; the notices that BOOTSEL and the
 * loader show are printed over them, so the rows are shared here. */
#define UI_ROW_PROMPT       23
#define UI_ROW_EXIT         24

#define UI_FORGOTTEN_MARK   '\001'  /* what a forgotten panel row holds      */

/* The menu (ui_menu): a title, one row to an item, and a hint at the bottom. */
#define UI_MENU_ROW_TITLE   1
#define UI_MENU_ROW_FIRST   3
#define UI_MENU_ROW_HINT    30
#define UI_MENU_MAX_ITEMS   (UI_MENU_ROW_HINT - UI_MENU_ROW_FIRST - 1)
#define UI_MENU_COL_MARK    1       /* the arrow in front of the chosen item */
#define UI_MENU_COL_TEXT    3
#define UI_MENU_COL_HEADING 1
#define UI_MENU_MARK        ">"
#define UI_MENU_HEADING     '#'     /* an item text that starts with this is a heading */
#define UI_POLL_MS         3       /* pause between looks at the keyboard   */
#define UI_REBOOT_MS        300     /* time the BOOTSEL notice stays visible */

#endif /* UI_CONST_H */
