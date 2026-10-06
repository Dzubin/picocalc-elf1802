/*
 * debugger.c - the debugger view. See debugger.h.
 *
 * Author: Thomas Dzubin
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "lcd.h"          /* RGB(), for the colours                           */
#include "keyboard.h"     /* KEY_ESC, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT   */

#include "debugger.h"
#include "debugger_const.h"
#include "isa.h"          /* the listing, the registers and the branch targets */
#include "source_gen.h"
#include "elf1802_const.h"  /* the colours and TCOLS                          */
#include "platform.h"     /* plat_audio_stop()                                */
#include "ui.h"

static elf_t   *machine;                        /* the Elf being debugged     */
static bool     active;                         /* the debugger is on the screen */
static bool     halted;                         /* the Elf is stopped for it  */
static bool     dirty;                          /* the screen is out of date  */
static bool     follow = true;                  /* the list starts at the PC  */
static uint16_t top;                            /* the address on the list's first line */
static int32_t  break_at = -1;                  /* the breakpoint, -1 for none */
static bool     broke;                          /* stopped by the breakpoint  */
static uint32_t step_cycles;                    /* cycles the last step took  */
static uint8_t  targets[ELF_RAM_SIZE / 8];      /* branch targets, a bit each */

void debugger_attach(elf_t *m)
{
    machine = m;
}

void debugger_reset(void)
{
    active = false;
    halted = false;
    broke = false;
    follow = true;
    break_at = -1;
}

bool    debugger_active(void)          { return active; }
bool    debugger_halted(void)          { return halted; }
bool    debugger_dirty(void)           { return dirty; }
int32_t debugger_break_address(void)   { return break_at; }

static uint16_t pc_address(void)
{
    return machine->cpu.r[machine->cpu.p];
}

/* Stop the Elf at the end of the instruction it is in, so that the PC and the
 * registers show a whole number of instructions. */
static void halt(void)
{
    if (machine->cpu.phase == CPU_PHASE_EXEC || machine->cpu.phase == CPU_PHASE_EXEC2)
        elf_step(machine);
    halted = true;
    follow = true;
    dirty = true;
    plat_audio_stop();                  /* nothing is playing while it is stopped */
    qaudio_flush(&machine->audio);
}

static void go(void)
{
    qaudio_flush(&machine->audio);
    halted = false;
    broke = false;
    dirty = true;
}

void debugger_enter(void)
{
    active = true;
    broke = false;
    halt();
}

void debugger_leave(void)
{
    active = false;
    go();
}

void debugger_break_hit(void)
{
    halt();
    broke = true;
}

bool debugger_wants_repeat(uint8_t code)
{
    return ui_lower(code) == DKEY_STEP || code == ' ' || code == KEY_UP ||
           code == KEY_DOWN || code == KEY_LEFT || code == KEY_RIGHT;
}

/* Author: Thomas Dzubin */
int debugger_key(uint8_t code)
{
    uint8_t key = ui_lower(code);
    uint8_t b[ISA_MAX_BYTES];
    char text[TCOLS];
    int k;

    if (code == KEY_ESC) {
        debugger_leave();
        return DEBUGGER_KEY_LEFT;
    }
    if (key == DKEY_STEP || code == ' ') {
        halt();
        broke = false;
        step_cycles = elf_step(machine);
        return DEBUGGER_KEY_USED;
    }
    if (key == DKEY_GO) {
        go();
        return DEBUGGER_KEY_USED;
    }
    if (key == DKEY_HALT) {
        halt();
        broke = false;
        return DEBUGGER_KEY_USED;
    }
    if (key == DKEY_BREAK) {
        break_at = (break_at == (int32_t)top) ? -1 : (int32_t)top;
        dirty = true;
        return DEBUGGER_KEY_USED;
    }
    if (key == DKEY_EDIT)
        return DEBUGGER_KEY_EDIT;
    if (key == DKEY_HOME) {
        follow = true;
        dirty = true;
        return DEBUGGER_KEY_USED;
    }
    if (code == KEY_UP || code == KEY_DOWN || code == KEY_LEFT || code == KEY_RIGHT) {
        if (follow)
            top = pc_address();
        follow = false;
        if (code == KEY_UP) {
            top = (uint16_t)(top - 1);
        } else if (code == KEY_DOWN) {
            for (k = 0; k < ISA_MAX_BYTES; k++)
                b[k] = elf_peek(machine, (uint16_t)(top + k));
            top = (uint16_t)(top + isa_1802.disasm(top, b, NULL, text, sizeof text));
        } else if (code == KEY_LEFT) {
            top = (uint16_t)(top - DKEY_PAGE);
        } else {
            top = (uint16_t)(top + DKEY_PAGE);
        }
        dirty = true;
        return DEBUGGER_KEY_USED;
    }
    return DEBUGGER_KEY_PASS;
}

/* The debugger screen: registers, the listing from the PC (or wherever it was
 * scrolled to), memory at the data pointer, and the keys. Only the rows whose
 * text changed are drawn.
 *
 * Author: Thomas Dzubin */
void debugger_draw(void)
{
    const cpu1802_t *c = &machine->cpu;
    char line[TCOLS + 1];
    char bytes[12];
    char text[16];
    char mark_bp[8];
    uint8_t b[ISA_MAX_BYTES];
    uint16_t pc = pc_address();
    uint16_t addr;
    size_t pos;
    int i, k;

    dirty = false;

    if (break_at >= 0)
        snprintf(mark_bp, sizeof mark_bp, "%04X", (unsigned)(break_at & 0xFFFF));
    else
        snprintf(mark_bp, sizeof mark_bp, "----");
    snprintf(line, sizeof line, "DEBUGGER  %s  BP:%s",
             broke ? "BREAK" : (halted ? "HALTED" : "RUNNING"), mark_bp);
    ui_panel_row(DBG_ROW_TITLE, 0, line, COL_CYAN);

    for (k = 0; isa_1802.register_row(c, k, line, sizeof line); k++)
        ui_panel_row(k == 0 ? DBG_ROW_FLAGS : DBG_ROW_REGS + k - 1, 0, line, COL_GREEN);

    addr = follow ? pc : top;
    top = addr;
    srcgen_mark_targets(&isa_1802, machine->ram, ELF_RAM_SIZE, targets);
    for (i = 0; i < DBG_LIST_LINES; i++) {
        int len;
        uint16_t colour;

        for (k = 0; k < ISA_MAX_BYTES; k++)
            b[k] = elf_peek(machine, (uint16_t)(addr + k));
        len = isa_1802.disasm(addr, b, NULL, text, sizeof text);
        pos = 0;
        bytes[0] = '\0';
        for (k = 0; k < len; k++)
            pos += (size_t)snprintf(bytes + pos, sizeof bytes - pos, "%s%02X",
                                    k ? " " : "", b[k]);
        /* a colon after the address marks the target of a branch or jump */
        snprintf(line, sizeof line, "%c%c%04X%c %-8s %s", addr == pc ? '>' : ' ',
                 (int32_t)addr == break_at ? '*' : ' ', (unsigned)addr,
                 (addr < ELF_RAM_SIZE && ((targets[addr >> 3] >> (addr & 7)) & 1)) ? ':' : ' ',
                 bytes, text);
        colour = (int32_t)addr == break_at ? COL_RED :
                 (addr == pc ? COL_YELLOW : COL_WHITE);
        ui_panel_row(DBG_ROW_LIST + i, 0, line, colour);
        addr = (uint16_t)(addr + len);
    }

    addr = (uint16_t)(c->r[c->x] & ~(DBG_DUMP_BYTES - 1));
    for (i = 0; i < DBG_DUMP_LINES; i++) {
        pos = (size_t)snprintf(line, sizeof line, "%04X:", (unsigned)addr);
        for (k = 0; k < DBG_DUMP_BYTES; k++)
            pos += (size_t)snprintf(line + pos, sizeof line - pos, " %02X",
                                    elf_peek(machine, (uint16_t)(addr + k)));
        ui_panel_row(DBG_ROW_DUMP + i, 0, line, COL_CYAN);
        addr = (uint16_t)(addr + DBG_DUMP_BYTES);
    }

    snprintf(line, sizeof line, "LAST STEP %u CYCLES  TOTAL %u",
             (unsigned)step_cycles, (unsigned)c->cycles);
    ui_panel_row(DBG_ROW_CYCLES, 0, line, COL_WHITE);

    ui_panel_row(DBG_ROW_HELP, 0, "S/SPC step  G go  H halt  P breakpoint", COL_WHITE);
    ui_panel_row(DBG_ROW_HELP + 1, 0, "ARROWS scroll  W follow PC  T editor", COL_WHITE);
    ui_panel_row(DBG_ROW_HELP + 2, 0, "* is P  + is R(X)   ESC: leave  F1 help", COL_WHITE);
}
