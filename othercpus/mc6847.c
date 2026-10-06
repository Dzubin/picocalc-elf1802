/*
 * mc6847.c - the Motorola MC6847 video display generator. See mc6847.h.
 *
 * Modes and timing follow the data sheet; the colours and the way the chip
 * lays out its character cell, its semigraphics and its "stripe" pattern when
 * external alphanumerics are asked for without a character ROM were checked
 * against MAME's mc6847.cpp.
 *
 * Author: Thomas Dzubin
 */
#include <string.h>

#include "mc6847.h"

typedef struct {
    uint8_t bytes;                  /* bytes in a line of the picture         */
    uint8_t rows;                   /* rows in the picture                    */
    uint8_t bits;                   /* bits per picture pixel: 2 or 1         */
    uint8_t wide;                   /* screen pixels per picture pixel        */
} graphics_mode_t;

static const graphics_mode_t graphics_modes[8] = M6847_GRAPHICS_MODES;
static const uint8_t font[M6847_FONT_CHARS][M6847_FONT_ROWS] = M6847_FONT;

void mc6847_reset(mc6847_t *v)
{
    int y;

    v->mode = 0;
    v->line = 0;
    v->cycles = 0;
    v->frames = 0;
    memset(v->rec_mode, 0, sizeof v->rec_mode);
    memset(v->rec_data, 0, sizeof v->rec_data);
    for (y = 0; y < M6847_HEIGHT; y++)
        v->dirty[y] = true;
}

/* Bytes of a line and the line's first address for the mode at display line y. */
static int line_address(uint8_t mode, int y, int *bytes)
{
    if (mode & M6847_AG) {
        const graphics_mode_t *g = &graphics_modes[(mode & M6847_GM_MASK) >> M6847_GM_SHIFT];

        *bytes = g->bytes;
        return (y / (M6847_HEIGHT / g->rows)) * g->bytes;
    }
    *bytes = M6847_COLUMNS;
    return (y / M6847_CELL_HEIGHT) * M6847_COLUMNS;
}

/* Record display line y: the mode pins now and the bytes it shows. */
static void record_line(mc6847_t *v, int y)
{
    uint8_t data[M6847_COLUMNS];
    uint8_t mode = (uint8_t)(v->mode & M6847_MODE_MASK);
    int bytes, addr, i;

    addr = line_address(mode, y, &bytes);
    memset(data, 0, sizeof data);
    for (i = 0; i < bytes; i++)
        data[i] = v->read ? v->read(v->ctx, (uint16_t)(addr + i)) : M6847_BYTE_NONE;
    if (mode != v->rec_mode[y] || memcmp(data, v->rec_data[y], sizeof data) != 0) {
        v->rec_mode[y] = mode;
        memcpy(v->rec_data[y], data, sizeof data);
        v->dirty[y] = true;
    }
}

static void end_of_line(mc6847_t *v)
{
    int y = v->line - M6847_TOP_BORDER;

    if (y >= 0 && y < M6847_HEIGHT)
        record_line(v, y);
    if (++v->line >= M6847_LINES) {
        v->line = 0;
        v->frames++;
    }
}

void mc6847_advance(mc6847_t *v, int n)
{
    v->cycles += n;
    while (v->cycles >= M6847_CYCLES_PER_LINE) {
        v->cycles -= M6847_CYCLES_PER_LINE;
        end_of_line(v);
    }
}

bool mc6847_take_dirty(mc6847_t *v, int y)
{
    bool d;

    if (y < 0 || y >= M6847_HEIGHT)
        return false;
    d = v->dirty[y];
    v->dirty[y] = false;
    return d;
}

uint8_t mc6847_border(const mc6847_t *v)
{
    if (v->mode & M6847_AG)
        return (uint8_t)((v->mode & M6847_CSS) ? M6847_PAL_CG_SET1 : M6847_PAL_CG_SET0);
    return M6847_BLACK;
}

bool mc6847_field_sync(const mc6847_t *v)
{
    return v->line < M6847_FS_LOW_FIRST || v->line > M6847_FS_LOW_LAST;
}

/* Eight pixels of a pattern, the leftmost in bit 7, in two colours. */
static void put_pattern(uint8_t *out, uint8_t pattern, uint8_t color_0, uint8_t color_1)
{
    int j;

    for (j = 0; j < 8; j++)
        out[j] = (pattern & (M6847_PATTERN_LEFTMOST >> j)) ? color_1 : color_0;
}

/* A graphics line: the bytes are two-bit pixels (four colours) or one-bit
 * pixels (two), each picture pixel drawn "wide" screen pixels across.
 *
 * Author: Thomas Dzubin */
static void render_graphics(const mc6847_t *v, int y, uint8_t *out)
{
    uint8_t mode = v->rec_mode[y];
    const graphics_mode_t *g = &graphics_modes[(mode & M6847_GM_MASK) >> M6847_GM_SHIFT];
    int css = (mode & M6847_CSS) != 0;
    int i, p, k, per_byte = 8 / g->bits;

    for (i = 0; i < g->bytes; i++) {
        uint8_t b = v->rec_data[y][i];

        for (p = 0; p < per_byte; p++) {
            uint8_t color;

            if (g->bits == 2)
                color = (uint8_t)((css ? M6847_PAL_CG_SET1 : M6847_PAL_CG_SET0) +
                                  ((b >> (6 - 2 * p)) & 3));
            else
                color = (uint8_t)((css ? M6847_PAL_RG_SET1 : M6847_PAL_RG_SET0) +
                                  ((b >> (7 - p)) & 1));
            for (k = 0; k < g->wide; k++)
                *out++ = color;
        }
    }
}

/* One cell of semigraphics: the line of the cell picks which row of blocks
 * (the 4 has two rows of blocks, the 6 has three) and each block is a nibble. */
static uint8_t semigraphics_pattern(uint8_t byte, int line, bool six)
{
    int slice = (M6847_CELL_HEIGHT - 1 - line) /
                (six ? M6847_SG6_SLICE_LINES : M6847_SG4_SLICE_LINES);
    uint8_t ch = (uint8_t)(byte & (six ? M6847_SG6_MASK : M6847_SG4_MASK));
    uint8_t right = (uint8_t)((ch >> (slice * 2)) & 1);
    uint8_t left = (uint8_t)((ch >> (slice * 2 + 1)) & 1);

    return (uint8_t)((left ? M6847_PATTERN_LEFT : 0x00) | (right ? M6847_PATTERN_RIGHT : 0x00));
}

/* Author: Thomas Dzubin */
void mc6847_render_line(const mc6847_t *v, int y, uint8_t *out)
{
    uint8_t mode;
    int line, css, fr, i;

    if (y < 0 || y >= M6847_HEIGHT) {               /* no such line: black */
        memset(out, M6847_BLACK, M6847_WIDTH);
        return;
    }
    mode = v->rec_mode[y];
    line = y % M6847_CELL_HEIGHT;
    css = (mode & M6847_CSS) != 0;
    fr = line - M6847_FONT_FIRST_LINE;
    if (mode & M6847_AG) {
        render_graphics(v, y, out);
        return;
    }
    for (i = 0; i < M6847_COLUMNS; i++) {
        uint8_t b = v->rec_data[y][i];
        uint8_t pattern, c0, c1;

        if (b & M6847_BYTE_AS) {                    /* semigraphics */
            bool six = (mode & M6847_INTEXT) != 0;

            pattern = semigraphics_pattern(b, line, six);
            c0 = M6847_BLACK;
            c1 = six ? (uint8_t)((css ? M6847_PAL_CG_SET1 : M6847_PAL_CG_SET0) +
                                 ((b >> M6847_SG6_COLOR_SHIFT) & M6847_SG6_COLOR_MASK))
                     : (uint8_t)((b >> M6847_SG4_COLOR_SHIFT) & M6847_SG4_COLOR_MASK);
        } else {
            c0 = (uint8_t)(css ? M6847_PAL_TEXT_SET1 : M6847_PAL_TEXT_SET0);
            c1 = (uint8_t)(c0 + 1);
            if (mode & M6847_INTEXT) {              /* no character ROM: stripes */
                pattern = (uint8_t)~(b & M6847_STRIPE_MASK);
            } else {
                pattern = 0;
                if (fr >= 0 && fr < M6847_FONT_ROWS)
                    pattern = (uint8_t)(font[b & M6847_CHAR_MASK][fr] << M6847_FONT_SHIFT);
                if (b & M6847_BYTE_INV)
                    pattern = (uint8_t)~pattern;
            }
        }
        put_pattern(out + i * 8, pattern, c0, c1);
    }
}
