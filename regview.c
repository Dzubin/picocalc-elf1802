/*
 * regview.c - the register read-out for an Elf with no 1861. See regview.h.
 *
 * Author: Thomas Dzubin
 */
#include <stdint.h>
#include <stdio.h>

#include "lcd.h"          /* RGB(), for the colours                           */

#include "isa.h"          /* the registers and the next instruction           */
#include "elf1802_const.h"  /* the colours and TCOLS                          */
#include "regview.h"
#include "regview_const.h"
#include "ui.h"

void regview_forget(void)
{
    ui_forget_row_span(0, REGVIEW_LAST_ROW);
}

/* Author: Thomas Dzubin */
void regview_update(const elf_t *m)
{
    const cpu1802_t *c = &m->cpu;
    char line[TCOLS + 1];
    char text[16];
    uint8_t b[ISA_MAX_BYTES];
    uint16_t pc = c->r[c->p];
    int row, k, len;
    size_t pos;

    ui_panel_row(REGVIEW_ROW_TITLE, 0, "BASIC ELF: 1802 ONLY (NO 1861 VIDEO)", COL_CYAN);

    for (row = 0; isa_1802.register_row(c, row, line, sizeof line); row++)
        ui_panel_row(row == 0 ? REGVIEW_ROW_FLAGS : REGVIEW_ROW_REGS + row - 1, 0, line,
                     COL_GREEN);

    for (k = 0; k < ISA_MAX_BYTES; k++)
        b[k] = elf_peek(m, (uint16_t)(pc + k));
    len = isa_1802.disasm(pc, b, NULL, text, sizeof text);
    pos = (size_t)snprintf(line, sizeof line, "NEXT %04X:", (unsigned)pc);
    for (k = 0; k < len; k++)
        pos += (size_t)snprintf(line + pos, sizeof line - pos, " %02X", b[k]);
    snprintf(line + pos, sizeof line - pos, "  %s", text);
    ui_panel_row(REGVIEW_ROW_NEXT, 0, line, COL_WHITE);

    snprintf(line, sizeof line, "CYCLES %u", (unsigned)c->cycles);
    ui_panel_row(REGVIEW_ROW_CYCLES, 0, line, COL_WHITE);

    ui_panel_row(REGVIEW_ROW_HINT, 0, "ESC then V: fit the 1861 video chip", COL_YELLOW);
}
