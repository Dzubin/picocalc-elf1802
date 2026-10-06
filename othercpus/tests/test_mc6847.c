/*
 * test_mc6847.c - checks of the MC6847 video chip (mc6847.c): the frame timing,
 * where the chip reads display memory in each mode, the picture it makes in
 * text, semigraphics 4 and 6 and every graphics mode, the colours, the border,
 * the field sync, a mode change in the middle of a frame and the marks for the
 * lines that changed. Plain C, builds with any compiler:
 *
 *     gcc -Wall -Wextra -I. -Iothercpus -DASM_IMAGE_SIZE=0x5000 \
 *         othercpus/tests/test_mc6847.c othercpus/mc6847.c -o test_mc6847
 *     ./test_mc6847
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "mc6847.h"

static int checks, failures;

#define CHECK(cond) do { \
        checks++; \
        if (!(cond)) { failures++; printf("FAIL line %d: %s\n", __LINE__, #cond); } \
    } while (0)

static uint8_t vram[8192];
static int last_addr, reads;
static mc6847_t vdg;

static uint8_t vram_read(void *ctx, uint16_t addr)
{
    (void)ctx;
    last_addr = addr;
    reads++;
    return vram[addr];
}

static void start(uint8_t mode)
{
    memset(vram, 0, sizeof vram);
    memset(&vdg, 0, sizeof vdg);
    vdg.read = vram_read;
    mc6847_reset(&vdg);
    vdg.mode = mode;
}

/* Run a whole frame so that every display line is recorded. */
static void frame(void)
{
    mc6847_advance(&vdg, M6847_LINES * M6847_CYCLES_PER_LINE);
}

static uint8_t px[M6847_WIDTH];

static void render(int y)
{
    mc6847_render_line(&vdg, y, px);
}

/* Author: Thomas Dzubin */
static void test_timing(void)
{
    int i;

    start(0);
    CHECK(M6847_LINES * M6847_CYCLES_PER_LINE == 14934);
    mc6847_advance(&vdg, M6847_CYCLES_PER_LINE - 1);
    CHECK(vdg.line == 0 && vdg.frames == 0);
    mc6847_advance(&vdg, 1);
    CHECK(vdg.line == 1);
    for (i = 1; i < M6847_LINES - 1; i++)
        mc6847_advance(&vdg, M6847_CYCLES_PER_LINE);
    CHECK(vdg.line == M6847_LINES - 1 && vdg.frames == 0);
    mc6847_advance(&vdg, M6847_CYCLES_PER_LINE);
    CHECK(vdg.line == 0 && vdg.frames == 1);

    /* any slicing of the cycles gives the same frames */
    start(0);
    for (i = 0; i < 14934 * 3; i++)
        mc6847_advance(&vdg, 1);
    CHECK(vdg.frames == 3 && vdg.line == 0 && vdg.cycles == 0);
    mc6847_advance(&vdg, 100);
    CHECK(vdg.line == 1 && vdg.cycles == 43);

    /* field sync is low for the 32 lines from 216 */
    start(0);
    for (i = 0; i < 215; i++)
        mc6847_advance(&vdg, M6847_CYCLES_PER_LINE);
    CHECK(vdg.line == 215 && mc6847_field_sync(&vdg));
    mc6847_advance(&vdg, M6847_CYCLES_PER_LINE);
    CHECK(vdg.line == 216 && !mc6847_field_sync(&vdg));
    for (i = 0; i < 31; i++)
        mc6847_advance(&vdg, M6847_CYCLES_PER_LINE);
    CHECK(vdg.line == 247 && !mc6847_field_sync(&vdg));
    mc6847_advance(&vdg, M6847_CYCLES_PER_LINE);
    CHECK(vdg.line == 248 && mc6847_field_sync(&vdg));
}

/* Author: Thomas Dzubin */
static void test_addresses(void)
{
    /* text: 32 bytes a row, each row for 12 lines */
    start(0);
    reads = 0;
    mc6847_advance(&vdg, M6847_TOP_BORDER * M6847_CYCLES_PER_LINE);
    CHECK(reads == 0);              /* nothing is read in the top border */
    mc6847_advance(&vdg, M6847_CYCLES_PER_LINE);
    CHECK(reads == 32 && last_addr == 31);
    mc6847_advance(&vdg, 11 * M6847_CYCLES_PER_LINE);
    CHECK(reads == 32 * 12 && last_addr == 31);     /* still row 0 */
    mc6847_advance(&vdg, M6847_CYCLES_PER_LINE);
    CHECK(last_addr == 63);                         /* row 1 */
    start(0);
    frame();
    CHECK(last_addr == 511);

    /* graphics: RG1 (16 bytes, 64 rows, 3 lines each) */
    start(M6847_AG | M6847_GM0);
    mc6847_advance(&vdg, (M6847_TOP_BORDER + 3) * M6847_CYCLES_PER_LINE);
    CHECK(last_addr == 15);                         /* three lines of row 0 */
    mc6847_advance(&vdg, M6847_CYCLES_PER_LINE);
    CHECK(last_addr == 31);
    start(M6847_AG | M6847_GM0);
    frame();
    CHECK(last_addr == 64 * 16 - 1);

    /* RG6 (32 bytes, 192 rows) */
    start(M6847_AG | M6847_GM0 | M6847_GM1 | M6847_GM2);
    frame();
    CHECK(last_addr == 192 * 32 - 1);

    /* RG3 (16 bytes, 192 rows) and CG2 (32 bytes, 64 rows) and RG2 (16, 96) */
    start(M6847_AG | M6847_GM2 | M6847_GM0);
    frame();
    CHECK(last_addr == 192 * 16 - 1);
    start(M6847_AG | M6847_GM1);
    frame();
    CHECK(last_addr == 64 * 32 - 1);
    start(M6847_AG | M6847_GM1 | M6847_GM0);
    frame();
    CHECK(last_addr == 96 * 16 - 1);
}

/* Author: Thomas Dzubin */
static void test_text(void)
{
    int i;

    /* the letter A (code 1) in the first cell: rows 3 to 9 of the cell, three pixels
     * across the top, bright on a dark background */
    start(0);
    vram[0] = 0x01;
    frame();
    render(0);
    for (i = 0; i < 8; i++)
        CHECK(px[i] == 12);
    render(3);                                      /* .###. at columns 2 to 6 */
    CHECK(px[2] == 12 && px[3] == 13 && px[4] == 13 && px[5] == 13 && px[6] == 12 && px[7] == 12);
    render(6);                                      /* #####: the crossbar */
    CHECK(px[1] == 12 && px[2] == 13 && px[6] == 13 && px[7] == 12);
    render(10);
    for (i = 0; i < 8; i++)
        CHECK(px[i] == 12);
    render(3);
    CHECK(px[8 + 3] == 13);                         /* the @ has a top row too */

    /* a blank (space is code 0x20) and the inverse of it */
    start(0);
    vram[0] = 0x20;
    vram[1] = 0x20 | M6847_BYTE_INV;
    frame();
    render(5);
    for (i = 0; i < 8; i++)
        CHECK(px[i] == 12 && px[8 + i] == 13);

    /* the orange set */
    start(M6847_CSS);
    vram[0] = 0x01;
    frame();
    render(3);
    CHECK(px[2] == 14 && px[3] == 15);

    /* without a character ROM, external alphanumerics make stripes of the code's bits */
    start(M6847_INTEXT);
    vram[0] = 0x55;
    frame();
    render(0);
    CHECK(px[0] == 13 && px[1] == 12 && px[2] == 13 && px[3] == 12 && px[7] == 12);
}

/* Author: Thomas Dzubin */
static void test_semigraphics(void)
{
    /* semigraphics 4: colour 3 in bits 4 to 6, blocks in bits 0 to 3
     * (top left 3, top right 2, bottom left 1, bottom right 0) */
    start(0);
    vram[0] = (uint8_t)(M6847_BYTE_AS | (3 << 4) | 0x0A);
    frame();
    render(0);
    CHECK(px[0] == 3 && px[3] == 3 && px[4] == 8 && px[7] == 8);        /* top left lit */
    render(5);
    CHECK(px[0] == 3 && px[4] == 8);
    render(6);
    CHECK(px[0] == 3 && px[3] == 3 && px[4] == 8 && px[7] == 8);        /* bottom left lit */
    start(0);
    vram[0] = (uint8_t)(M6847_BYTE_AS | (7 << 4) | 0x05);               /* top right, bottom right */
    frame();
    render(2);
    CHECK(px[0] == 8 && px[4] == 7);
    render(9);
    CHECK(px[0] == 8 && px[4] == 7);

    /* semigraphics 6: six blocks 4 pixels wide and 4 lines tall; colour from bits 7 and 6
     * plus 4 with the second colour set */
    start(M6847_INTEXT);
    vram[0] = (uint8_t)(M6847_BYTE_AS | 0x40 | 0x01);                   /* bottom right only */
    frame();
    render(0);
    CHECK(px[0] == 8 && px[4] == 8);
    render(11);
    CHECK(px[0] == 8 && px[3] == 8 && px[4] == 3 && px[7] == 3);
    render(8);
    CHECK(px[4] == 3);
    render(7);
    CHECK(px[4] == 8);
    start(M6847_INTEXT | M6847_CSS);
    vram[0] = (uint8_t)(M6847_BYTE_AS | 0x20);                          /* top left */
    frame();
    render(0);
    CHECK(px[0] == 6 && px[3] == 6 && px[4] == 8);
}

/* Author: Thomas Dzubin */
static void test_graphics(void)
{
    int i;

    /* RG6: 256 x 192, two colours, one pixel each; bits left to right */
    start(M6847_AG | M6847_GM0 | M6847_GM1 | M6847_GM2);
    vram[0] = 0xA5;
    vram[32] = 0xFF;
    frame();
    render(0);
    CHECK(px[0] == 9 && px[1] == 8 && px[2] == 9 && px[3] == 8 && px[4] == 8 && px[5] == 9 &&
          px[6] == 8 && px[7] == 9);
    render(1);
    for (i = 0; i < 8; i++)
        CHECK(px[i] == 9);
    CHECK(px[8] == 8);

    /* the second colour set is black and buff */
    start(M6847_AG | M6847_GM0 | M6847_GM1 | M6847_GM2 | M6847_CSS);
    vram[0] = 0x80;
    frame();
    render(0);
    CHECK(px[0] == 11 && px[1] == 10);

    /* CG6: 128 x 192, four colours, two screen pixels wide */
    start(M6847_AG | M6847_GM1 | M6847_GM2);
    vram[0] = 0x1B;                                 /* 00 01 10 11 */
    frame();
    render(0);
    CHECK(px[0] == 0 && px[1] == 0 && px[2] == 1 && px[3] == 1 && px[4] == 2 && px[5] == 2 &&
          px[6] == 3 && px[7] == 3);
    start(M6847_AG | M6847_GM1 | M6847_GM2 | M6847_CSS);
    vram[0] = 0x1B;
    frame();
    render(0);
    CHECK(px[0] == 4 && px[2] == 5 && px[4] == 6 && px[6] == 7);

    /* CG1: 64 x 64, four colours, four screen pixels wide, 16 bytes a row */
    start(M6847_AG);
    vram[0] = 0x1B;
    vram[15] = 0xC0;
    frame();
    render(0);
    CHECK(px[0] == 0 && px[3] == 0 && px[4] == 1 && px[7] == 1 && px[8] == 2 && px[12] == 3 && px[15] == 3);
    render(2);
    CHECK(px[0] == 0 && px[4] == 1);                /* the same row for three lines */
    CHECK(px[240] == 3 && px[255] == 0);
    render(3);
    CHECK(px[0] == 0 && px[4] == 0);                /* the next row is empty */

    /* RG1: 128 x 64, two colours, two screen pixels wide */
    start(M6847_AG | M6847_GM0);
    vram[0] = 0x40;
    frame();
    render(0);
    CHECK(px[0] == 8 && px[1] == 8 && px[2] == 9 && px[3] == 9 && px[4] == 8);

    /* the picture covers all 256 pixels in every graphics mode */
    for (i = 0; i < 8; i++) {
        int x;

        start((uint8_t)(M6847_AG | (i << 1)));
        memset(vram, 0xFF, sizeof vram);
        frame();
        memset(px, 0xEE, sizeof px);
        render(100);
        for (x = 0; x < M6847_WIDTH; x++)
            CHECK(px[x] != 0xEE);
    }
}

/* Author: Thomas Dzubin */
static void test_border_and_changes(void)
{
    int y;

    start(0);
    CHECK(mc6847_border(&vdg) == 8);
    vdg.mode = M6847_AG;
    CHECK(mc6847_border(&vdg) == 0);
    vdg.mode = M6847_AG | M6847_CSS;
    CHECK(mc6847_border(&vdg) == 4);

    /* a mode change part way down the frame takes effect from the line it happens on */
    start(0);
    vram[0] = 0x01;
    mc6847_advance(&vdg, (M6847_TOP_BORDER + 100) * M6847_CYCLES_PER_LINE);
    vdg.mode = M6847_AG | M6847_GM0 | M6847_GM1 | M6847_GM2;
    mc6847_advance(&vdg, 92 * M6847_CYCLES_PER_LINE);
    CHECK(vdg.rec_mode[99] == 0);
    CHECK(vdg.rec_mode[100] == (M6847_AG | M6847_GM0 | M6847_GM1 | M6847_GM2));
    CHECK(vdg.rec_mode[191] == (M6847_AG | M6847_GM0 | M6847_GM1 | M6847_GM2));

    /* every line starts marked; clearing the marks and changing one byte of a text row marks the
     * twelve lines of that row only */
    start(0);
    frame();
    for (y = 0; y < M6847_HEIGHT; y++)
        CHECK(mc6847_take_dirty(&vdg, y));
    for (y = 0; y < M6847_HEIGHT; y++)
        CHECK(!mc6847_take_dirty(&vdg, y));
    frame();
    for (y = 0; y < M6847_HEIGHT; y++)
        CHECK(!mc6847_take_dirty(&vdg, y));         /* nothing changed */
    vram[40] = 0x21;                                /* row 1, column 8 */
    frame();
    for (y = 0; y < M6847_HEIGHT; y++)
        CHECK(mc6847_take_dirty(&vdg, y) == (y >= 12 && y < 24));
}

/* A line number outside the picture is harmless. */
static void test_out_of_range(void)
{
    int i, black = 0;

    start(0);
    frame();
    CHECK(!mc6847_take_dirty(&vdg, -1));
    CHECK(!mc6847_take_dirty(&vdg, M6847_HEIGHT));
    memset(px, 0x55, sizeof px);
    mc6847_render_line(&vdg, -1, px);
    for (i = 0; i < M6847_WIDTH; i++)
        black += px[i] == M6847_BLACK;
    CHECK(black == M6847_WIDTH);
    memset(px, 0x55, sizeof px);
    mc6847_render_line(&vdg, M6847_HEIGHT, px);
    for (black = 0, i = 0; i < M6847_WIDTH; i++)
        black += px[i] == M6847_BLACK;
    CHECK(black == M6847_WIDTH);
}

int main(void)
{
    test_timing();
    test_addresses();
    test_text();
    test_semigraphics();
    test_graphics();
    test_border_and_changes();
    test_out_of_range();
    printf("%d checks, %d failed\n", checks, failures);
    return failures != 0;
}
