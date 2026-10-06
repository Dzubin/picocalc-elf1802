/*
 * elfpanel.c - the Elf II front panel as drawn on the PicoCalc: the title, the
 * two seven-segment hex displays, the Q LED, the 4 x 4 hex keypad, the RUN, LOAD
 * and M/P toggles and the IN button, with the tag that says ASCII (and F4=OFF) or hints at F4.
 * It draws only what changed. See elfpanel.h.
 *
 * Author: Thomas Dzubin
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "lcd.h"          /* lcd_solid_rectangle(), lcd_blit(), WIDTH, RGB()  */

#include "elf1802_const.h"  /* the colours, the panel's geometry and glyphs   */
#include "elfpanel.h"

static int32_t  panel_keys_shown = -1;    /* hex keys drawn lit, a bit each; -1 = unknown */
static int32_t  panel_in_shown = -1;      /* IN drawn pressed, 0 or 1        */
static int32_t  panel_sw_shown = -1;      /* switches as drawn, a bit each   */
static int32_t  panel_tag_shown = -1;     /* the tag as drawn: 0 none, 1 ASCII, 2 up the hint's pulse */
static uint32_t panel_polls_seen;         /* the machine's keyboard polls at the last look */
static uint32_t panel_hint_until_ms;      /* the hint shows until then       */
static int      shown_display = -1;       /* hex display value, -1 = unknown */
static int      shown_q = -1;             /* Q LED state, -1 = unknown       */
static uint16_t cv[PANEL_CANVAS_PIXELS];  /* a panel element, before it is blitted */
static int      cv_w, cv_h;

void elfpanel_forget(void)
{
    shown_display = shown_q = -1;
    panel_keys_shown = panel_in_shown = panel_sw_shown = panel_tag_shown = -1;
}

/* ===================================================================== */
/*  The front panel: hex displays, Q LED, switches                        */
/* ===================================================================== */
/* One hex digit as a seven-segment display. */
void elfpanel_draw_hex_digit(int x, int y, unsigned value)
{
    static const uint8_t rect[SEG_COUNT][4] = SEGMENT_RECTS;
    static const uint8_t lit[16] = SEGMENT_DIGITS;
    int s;

    lcd_solid_rectangle(COL_BG, (uint16_t)x, (uint16_t)y, DIGIT_W, DIGIT_H);
    for (s = 0; s < SEG_COUNT; s++)
        if ((lit[value & 15] >> s) & 1)
            lcd_solid_rectangle(COL_LED, (uint16_t)(x + rect[s][SEG_X]),
                                (uint16_t)(y + rect[s][SEG_Y]),
                                rect[s][SEG_W], rect[s][SEG_H]);
}

/* The Q LED: a white-framed square, red inside when Q is on. */
void elfpanel_draw_q_led(int x, int y, int size, int border, bool on)
{
    lcd_solid_rectangle(COL_WHITE, (uint16_t)x, (uint16_t)y, (uint16_t)size,
                        (uint16_t)size);
    lcd_solid_rectangle(on ? COL_LED : COL_BG, (uint16_t)(x + border),
                        (uint16_t)(y + border), (uint16_t)(size - 2 * border),
                        (uint16_t)(size - 2 * border));
}


/* ===================================================================== */
/*  The Elf II panel screen                                               */
/* ===================================================================== */
/* A panel element is drawn into the canvas (cv) and blitted in one go. */
static void cv_begin(int w, int h, uint16_t background)
{
    int i;

    if (w * h > PANEL_CANVAS_PIXELS)    /* never: every element fits (see the constants) */
        h = PANEL_CANVAS_PIXELS / w;
    cv_w = w;
    cv_h = h;
    for (i = 0; i < w * h; i++)
        cv[i] = background;
}

static void cv_rect(int x, int y, int w, int h, uint16_t colour)
{
    int xx, yy;

    for (yy = y; yy < y + h && yy < cv_h; yy++) {
        if (yy < 0)
            continue;
        for (xx = x; xx < x + w && xx < cv_w; xx++)
            if (xx >= 0)
                cv[yy * cv_w + xx] = colour;
    }
}

/* A rectangle with half circles for its short ends. */
static void cv_pill(int x, int y, int w, int h, uint16_t colour)
{
    int r = h / 2;
    int yy, xx;

    for (yy = 0; yy < h; yy++) {
        int d2 = (2 * yy + 1 - h) * (2 * yy + 1 - h);     /* 4 * d squared */
        int inset = 0;

        while (inset < r &&
               (2 * r - 2 * inset - 1) * (2 * r - 2 * inset - 1) + d2 > 4 * r * r)
            inset++;
        for (xx = inset; xx < w - inset; xx++)
            cv_rect(x + xx, y + yy, 1, 1, colour);
    }
}

static const panel_glyph_t *glyph_for(char ch)
{
    static const panel_glyph_t glyphs[] = PANEL_GLYPHS;
    size_t n = sizeof glyphs / sizeof glyphs[0];
    size_t i;

    for (i = 0; i < n; i++)
        if (glyphs[i].ch == ch)
            return &glyphs[i];
    return &glyphs[n - 1];              /* the last one is a blank */
}

static void cv_text(int x, int y, const char *str, int scale, uint16_t colour)
{
    for (; *str; str++) {
        const panel_glyph_t *g = glyph_for(*str);
        int r, c;

        for (r = 0; r < PANEL_GLYPH_H; r++)
            for (c = 0; c < PANEL_GLYPH_W; c++)
                if ((g->rows[r] >> (PANEL_GLYPH_W - 1 - c)) & 1)
                    cv_rect(x + c * scale, y + r * scale, scale, scale, colour);
        x += PANEL_GLYPH_ADVANCE * scale;
    }
}

static void cv_blit(int x, int y)
{
    lcd_blit(cv, (uint16_t)x, (uint16_t)y, (uint16_t)cv_w, (uint16_t)cv_h);
}

/* One key of the keypad: a light cap with a dark digit, or (held down) a black
 * cap with a white frame and digit.
 *
 * Author: Thomas Dzubin */
static void panel_draw_key(int index, bool lit)
{
    static const uint8_t values[16] = PANEL_KEY_VALUES;
    char label[2];

    label[0] = "0123456789ABCDEF"[values[index]];
    label[1] = '\0';
    cv_begin(PANEL_KEY_W, PANEL_KEY_H, lit ? COL_BG : COL_KEYCAP);
    if (lit) {
        cv_rect(0, 0, PANEL_KEY_W, PANEL_KEY_BORDER, COL_WHITE);
        cv_rect(0, PANEL_KEY_H - PANEL_KEY_BORDER, PANEL_KEY_W, PANEL_KEY_BORDER, COL_WHITE);
        cv_rect(0, 0, PANEL_KEY_BORDER, PANEL_KEY_H, COL_WHITE);
        cv_rect(PANEL_KEY_W - PANEL_KEY_BORDER, 0, PANEL_KEY_BORDER, PANEL_KEY_H, COL_WHITE);
    } else {
        cv_rect(0, PANEL_KEY_H - PANEL_KEY_EDGE, PANEL_KEY_W, PANEL_KEY_EDGE, COL_KEYCAP_EDGE);
    }
    cv_text(PANEL_KEY_LABEL_X, PANEL_KEY_LABEL_Y, label, PANEL_KEY_LABEL_SCALE,
            lit ? COL_WHITE : COL_KEY_LABEL);
    cv_blit(PANEL_KEY_X + (index % 4) * PANEL_KEY_PITCH_X,
            PANEL_KEY_Y + (index / 4) * PANEL_KEY_PITCH_Y);
}

/* A toggle switch: a pivot plate and a lever with a ball on its end, up or
 * down, in the colour given. cx and cy are the centre of the pivot. */
static void panel_draw_toggle(int cx, int cy, bool up, uint16_t lever)
{
    int mx = PANEL_SW_CANVAS_W / 2;
    int my = PANEL_SW_CANVAS_H / 2;

    cv_begin(PANEL_SW_CANVAS_W, PANEL_SW_CANVAS_H, COL_BG);
    cv_rect(mx - 6, my - 6, 12, 12, COL_WHITE);
    cv_rect(mx - 3, my - 3, 6, 6, COL_BG);
    if (up) {
        cv_rect(mx - 2, my - 12, 4, 6, lever);
        cv_rect(mx - 4, my - 16, 8, 6, lever);
    } else {
        cv_rect(mx - 2, my + 6, 4, 6, lever);
        cv_rect(mx - 4, my + 10, 8, 6, lever);
    }
    cv_blit(cx - mx, cy - my);
}

/* The IN button: a white pill, or a black one with a white rim while held. */
static void panel_draw_in(bool pressed)
{
    cv_begin(PANEL_IN_W, PANEL_IN_H, COL_BG);
    cv_pill(0, 0, PANEL_IN_W, PANEL_IN_H, COL_WHITE);
    if (pressed)
        cv_pill(2, 2, PANEL_IN_W - 4, PANEL_IN_H - 4, COL_BG);
    cv_text((PANEL_IN_W - (2 * PANEL_GLYPH_ADVANCE - 1) * 2) / 2,
            (PANEL_IN_H - PANEL_GLYPH_H * 2) / 2, "IN", 2,
            pressed ? COL_WHITE : COL_KEY_LABEL);
    cv_blit(PANEL_IN_X, PANEL_IN_Y);
}

/* The width of a string in the panel letters at double size. */
static int text_width_x2(const char *text)
{
    return (int)strlen(text) * PANEL_GLYPH_ADVANCE * 2 - 2;
}

/* A pill over the whole tag with two lines of text in it, centred. */
static void draw_two_line_pill(uint16_t colour, const char *line1, const char *line2)
{
    int gap = (PANEL_TAG_H - 2 * PANEL_GLYPH_H * 2) / 3;

    cv_pill(0, 0, PANEL_TAG_W, PANEL_TAG_H, colour);
    cv_text((PANEL_TAG_W - text_width_x2(line1)) / 2, gap, line1, 2, COL_KEY_LABEL);
    cv_text((PANEL_TAG_W - text_width_x2(line2)) / 2, 2 * gap + PANEL_GLYPH_H * 2, line2, 2,
            COL_KEY_LABEL);
}

/* The tag under the Q LED: nothing (0), the yellow ASCII pill that says how to
 * leave that mode (1), or the pill that hints at F4, whose yellow is step
 * (tag - 2) of KBD_HINT_LEVELS.
 *
 * Author: Thomas Dzubin */
static void panel_draw_tag(int tag)
{
    cv_begin(PANEL_TAG_W, PANEL_TAG_H, COL_BG);
    if (tag == 1) {
        draw_two_line_pill(COL_YELLOW, PANEL_ASCII_TEXT, PANEL_ASCII_EXIT);
    } else if (tag >= 2) {
        int level = tag - 2;
        uint16_t colour = RGB(
            KBD_HINT_LOW_R + (KBD_HINT_HIGH_R - KBD_HINT_LOW_R) * level / KBD_HINT_LEVELS,
            KBD_HINT_LOW_G + (KBD_HINT_HIGH_G - KBD_HINT_LOW_G) * level / KBD_HINT_LEVELS,
            KBD_HINT_LOW_B + (KBD_HINT_HIGH_B - KBD_HINT_LOW_B) * level / KBD_HINT_LEVELS);

        draw_two_line_pill(colour, PANEL_HINT_LINE_1, PANEL_HINT_LINE_2);
    }
    cv_blit(PANEL_TAG_X, PANEL_TAG_Y);
}

/* Which tag to show now: the ASCII pill while the keys go to that keyboard,
 * otherwise the hint while a program has lately asked for it, pulsing slowly. */
static int panel_tag_wanted(const elfpanel_state_t *s)
{
    uint32_t now_ms = s->now_ms;

    if (s->kbd_polls != panel_polls_seen) {
        panel_polls_seen = s->kbd_polls;
        panel_hint_until_ms = now_ms + KBD_HINT_HOLD_MS;
    }
    if (s->ascii_mode)
        return 1;
    if ((int32_t)(panel_hint_until_ms - now_ms) > 0) {
        uint32_t half = KBD_HINT_PERIOD_MS / 2;
        uint32_t phase = now_ms % KBD_HINT_PERIOD_MS;
        uint32_t ramp = phase < half ? phase : KBD_HINT_PERIOD_MS - phase;

        return 2 + (int)(ramp * KBD_HINT_LEVELS / half);
    }
    return 0;
}

/* The parts of the panel that never change: the title and the switch names.
 *
 * Author: Thomas Dzubin */
void elfpanel_draw_static(void)
{
    static const char *const names[3] = { "RUN", "LOAD", "M/P" };
    static const int pivots[3] = { PANEL_SW_RUN_Y, PANEL_SW_LOAD_Y, PANEL_SW_MP_Y };
    int i;

    cv_begin((int)(sizeof PANEL_TITLE_TEXT - 1) * PANEL_GLYPH_ADVANCE * PANEL_TITLE_SCALE,
             PANEL_GLYPH_H * PANEL_TITLE_SCALE, COL_BG);
    cv_text(0, 0, PANEL_TITLE_TEXT, PANEL_TITLE_SCALE, COL_WHITE);
    cv_blit(PANEL_TITLE_X, PANEL_TITLE_Y);
    for (i = 0; i < 3; i++) {
        /* the first letter is the key that flips the switch: underlined */
        cv_begin(PANEL_SW_LABEL_W, PANEL_SW_LABEL_H, COL_BG);
        cv_text(0, 0, names[i], 1, COL_WHITE);
        cv_rect(0, PANEL_SW_UNDERLINE_Y, PANEL_GLYPH_W, 1, COL_WHITE);
        cv_blit(PANEL_SW_LABEL_X, pivots[i] + PANEL_SW_LABEL_DY);
    }
    cv_begin((int)(sizeof PANEL_MENU_LABEL + sizeof PANEL_MENU_KEY - 2) * PANEL_GLYPH_ADVANCE,
             PANEL_GLYPH_H, COL_BG);
    cv_text(0, 0, PANEL_MENU_LABEL, 1, COL_WHITE);
    cv_text((int)(sizeof PANEL_MENU_LABEL - 1) * PANEL_GLYPH_ADVANCE, 0, PANEL_MENU_KEY, 1,
            COL_YELLOW);
    cv_blit(PANEL_MENU_X, PANEL_MENU_Y);
}

/* Bring the parts of the panel that move up to date, drawing only what
 * changed (everything when force is set).
 *
 * Author: Thomas Dzubin */
void elfpanel_update(const elfpanel_state_t *s, bool force)
{
    static const uint8_t values[16] = PANEL_KEY_VALUES;
    uint32_t lit = s->lit_keys;
    int in = s->in_down ? 1 : 0;
    int sw = (s->sw_run ? 1 : 0) | (s->sw_load ? 2 : 0) | (s->sw_mp ? 4 : 0);
    bool q = s->q;
    int tag;
    int i;

    if (force || shown_display != s->display) {
        shown_display = s->display;
        elfpanel_draw_hex_digit(PANEL_DISPLAY_X_HIGH, PANEL_DISPLAY_Y, (unsigned)(s->display >> 4));
        elfpanel_draw_hex_digit(PANEL_DISPLAY_X_LOW, PANEL_DISPLAY_Y, (unsigned)(s->display & 15));
    }
    if (force || shown_q != (int)q) {
        shown_q = q;
        elfpanel_draw_q_led(PANEL_Q_X, PANEL_Q_Y, PANEL_Q_SIZE, PANEL_Q_BORDER, q);
    }
    if (panel_keys_shown != (int32_t)lit) {
        uint32_t changed = panel_keys_shown < 0 ? 0xFFFFu : (lit ^ (uint32_t)panel_keys_shown);

        for (i = 0; i < 16; i++)
            if ((changed >> values[i]) & 1u)
                panel_draw_key(i, (lit >> values[i]) & 1u);
        panel_keys_shown = (int32_t)lit;
    }
    if (panel_in_shown != in) {
        panel_in_shown = in;
        panel_draw_in(in != 0);
    }
    tag = panel_tag_wanted(s);
    if (panel_tag_shown != tag) {
        panel_tag_shown = tag;
        panel_draw_tag(tag);
    }
    if (panel_sw_shown != sw) {
        panel_sw_shown = sw;
        panel_draw_toggle(PANEL_SW_X, PANEL_SW_RUN_Y, s->sw_run, s->sw_run ? COL_GREEN : COL_WHITE);
        panel_draw_toggle(PANEL_SW_X, PANEL_SW_LOAD_Y, s->sw_load, s->sw_load ? COL_GREEN : COL_WHITE);
        panel_draw_toggle(PANEL_SW_X, PANEL_SW_MP_Y, s->sw_mp, s->sw_mp ? COL_YELLOW : COL_WHITE);
    }
}

