/*
 * test_mc10.c - checks of the TRS-80 MC-10 machine (mc10.c): the memory map, the
 * message program shown when no ROM is fitted, a ROM fitted from a file, the
 * keyboard matrix through 0xBFFF and port 2, the video mode pins, the sound bit,
 * the cassette input, the video chip kept in step, and the keys by character.
 * Plain C, builds with any compiler:
 *
 *     gcc -Wall -Wextra -I. -Iothercpus -DASM_IMAGE_SIZE=0x5000 \
 *         othercpus/tests/test_mc10.c othercpus/mc10.c othercpus/cpu6803.c \
 *         othercpus/mc6847.c membus.c rom.c qaudio.c asm_core.c othercpus/isa_6800.c \
 *         -o test_mc10
 *     ./test_mc10
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "asm_core.h"
#include "isa_others.h"
#include "mc10.h"
#include "platform.h"
#include "rom.h"

static int checks, failures;

#define CHECK(cond) do { \
        checks++; \
        if (!(cond)) { failures++; printf("FAIL line %d: %s\n", __LINE__, #cond); } \
    } while (0)

/* A fake SD card with one file, for the ROM image. */
static uint8_t card_data[MC10_ROM_SIZE];
static uint32_t card_len;

int plat_file_save(const char *name, const uint8_t *data, uint32_t len)
{
    (void)name; (void)data; (void)len;
    return PLAT_FILE_OK;
}

int plat_file_load(const char *name, uint8_t *data, uint32_t max, uint32_t *len)
{
    *len = 0;
    if (strcmp(name, "MC10.ROM") != 0)
        return PLAT_FILE_NOT_FOUND;
    *len = card_len < max ? card_len : max;
    memcpy(data, card_data, *len);
    return PLAT_FILE_OK;
}

static mc10_t m;
static asm_t assembler;
static uint8_t px[M6847_WIDTH];

/* Assemble source (it starts at ORG $E000) into the window of the ROM and make it
 * the ROM at 0xE000 with the reset vector pointing at it. */
static void fit_program(const char *source)
{
    int errors = asm_assemble(&isa_6803, &assembler, MC10_ROM_BASE, MC10_ROM_SIZE, source,
                              strlen(source));

    CHECK(errors == 0);
    memset(card_data, 0xFF, sizeof card_data);
    memcpy(card_data, assembler.image, (size_t)assembler.bytes);
    card_data[MC10_ROM_SIZE - 2] = (uint8_t)(MC10_ROM_BASE >> 8);
    card_data[MC10_ROM_SIZE - 1] = (uint8_t)MC10_ROM_BASE;
    card_len = MC10_ROM_SIZE;
    CHECK(rom_set(0, "MC10.ROM", MC10_ROM_BASE) == ROM_OK);
}

static void boot(uint32_t ram)
{
    mc10_init(&m, ram);
}

static void frame(void)
{
    mc10_run(&m, M6847_LINES * M6847_CYCLES_PER_LINE);
}

/* Author: Thomas Dzubin */
static void test_no_rom(void)
{
    const char *message = "NO MC-10 ROM FITTED";
    int i, nonzero = 0;

    rom_clear(0);
    boot(MC10_RAM_4K);
    CHECK(m.cpu.pc == MC10_FALLBACK_PAGE);
    mc10_run(&m, 5000);
    for (i = 0; message[i]; i++) {
        int c = message[i];

        c = c >= 0x40 ? c - 0x40 : c;
        CHECK(mc10_peek(&m, (uint16_t)(MC10_RAM_BASE + 6 * 32 + 6 + i)) == c);
    }
    CHECK(mc10_peek(&m, MC10_RAM_BASE + 8 * 32 + 2) == 'E' - 0x40);      /* "ESC R: ..." */
    CHECK(m.cpu.pc >= MC10_FALLBACK_PAGE && m.cpu.pc < MC10_FALLBACK_PAGE + 0x100);

    /* the message reaches the picture: the N of "NO" at row 6, column 6 */
    frame();
    mc6847_render_line(&m.vdg, 6 * 12 + 4, px);
    for (i = 0; i < 8; i++)
        nonzero += px[6 * 8 + i] == 13;
    CHECK(nonzero >= 2);
}

/* Author: Thomas Dzubin */
static void test_memory_map(void)
{
    rom_clear(0);
    boot(MC10_RAM_4K);
    membus_write(&m.bus, 0x4000, 0x5A);
    membus_write(&m.bus, 0x4FFF, 0xA5);
    CHECK(mc10_peek(&m, 0x4000) == 0x5A && mc10_peek(&m, 0x4FFF) == 0xA5);
    membus_write(&m.bus, 0x5000, 0x11);
    CHECK(mc10_peek(&m, 0x5000) == MC10_OPEN_BUS);          /* only 4K fitted */
    membus_write(&m.bus, 0x2000, 0x11);
    CHECK(mc10_peek(&m, 0x2000) == MC10_OPEN_BUS);          /* nothing below the RAM */

    boot(MC10_RAM_20K);
    membus_write(&m.bus, 0x5000, 0x11);
    membus_write(&m.bus, 0x8FFF, 0x22);
    CHECK(mc10_peek(&m, 0x5000) == 0x11 && mc10_peek(&m, 0x8FFF) == 0x22);
    membus_write(&m.bus, 0x9000, 0x11);
    CHECK(mc10_peek(&m, 0x9000) == MC10_OPEN_BUS);

    /* the port page: only 0xBFFF is the port */
    CHECK(membus_read(&m.bus, 0xBF00) == MC10_OPEN_BUS);
    CHECK(membus_read(&m.bus, 0xBFFE) == MC10_OPEN_BUS);
    CHECK(membus_read(&m.bus, 0xBFFF) == 0xFF);              /* no key down */

    /* a ROM from a file is read-only memory at 0xE000 */
    fit_program("ORG $E000\n NOP\nhold: BRA hold\n");
    boot(MC10_RAM_4K);
    CHECK(m.cpu.pc == MC10_ROM_BASE);
    CHECK(mc10_peek(&m, MC10_ROM_BASE) == 0x01);
    membus_write(&m.bus, MC10_ROM_BASE, 0x77);
    CHECK(mc10_peek(&m, MC10_ROM_BASE) == 0x01);
    mc10_run(&m, 100);
    CHECK(m.cpu.pc == MC10_ROM_BASE + 1);
    rom_clear(0);
}

/* Author: Thomas Dzubin */
static void test_keyboard(void)
{
    /* the program selects row 0, reads the port, then row 7 and reads port 2 */
    fit_program("ORG $E000\n"
                " LDAA #$FF\n STAA $00\n"                 /* port 1 all outputs */
                " LDAA #$FE\n STAA $02\n"                 /* row 0 */
                " LDAA $BFFF\n STAA $4000\n"
                " LDAA #$FD\n STAA $02\n"                 /* row 1 */
                " LDAA $BFFF\n STAA $4001\n"
                " LDAA #$7F\n STAA $02\n"                 /* row 7 */
                " LDAA $03\n STAA $4002\n"                /* port 2 */
                " LDAA #$00\n STAA $02\n"                 /* every row at once */
                " LDAA $BFFF\n STAA $4003\n"
                "hold: BRA hold\n");
    boot(MC10_RAM_4K);
    CHECK(mc10_press_char(&m, 'h', true));                 /* row 0, bit 1 */
    CHECK(mc10_press_char(&m, 'I', true));                 /* row 1, bit 1 */
    CHECK(mc10_press_char(&m, MC10_KEY_SHIFT, true));      /* row 7, bit 6 */
    mc10_run(&m, 400);
    CHECK(mc10_peek(&m, 0x4000) == 0xFD);
    CHECK(mc10_peek(&m, 0x4001) == 0xFD);
    CHECK((mc10_peek(&m, 0x4002) & MC10_P2_KEY_LINE) == 0);    /* shift pulls port 2 bit 1 low */
    CHECK(mc10_peek(&m, 0x4003) == 0xBD);                      /* all rows ANDed: H, I and shift */
    rom_clear(0);

    /* with nothing down everything reads high */
    fit_program("ORG $E000\n"
                " LDAA #$FF\n STAA $00\n LDAA #$00\n STAA $02\n"
                " LDAA $BFFF\n STAA $4000\n LDAA $03\n STAA $4001\n"
                "hold: BRA hold\n");
    boot(MC10_RAM_4K);
    mc10_run(&m, 300);
    CHECK(mc10_peek(&m, 0x4000) == 0xFF);
    CHECK((mc10_peek(&m, 0x4001) & MC10_P2_KEY_LINE) != 0);
    rom_clear(0);
}

/* Author: Thomas Dzubin */
static void test_keys_by_character(void)
{
    boot(MC10_RAM_4K);
    CHECK(mc10_press_char(&m, 'a', true));
    CHECK(m.keys[1] == 0x01);
    CHECK(mc10_press_char(&m, 'a', false));
    CHECK(m.keys[1] == 0);
    CHECK(mc10_press_char(&m, '!', true));                 /* SHIFT 1 */
    CHECK(m.keys[1] == 0x10 && m.keys[7] == MC10_KEYLINE_BIT);
    CHECK(mc10_press_char(&m, '!', false));
    CHECK(m.keys[1] == 0 && m.keys[7] == 0);
    CHECK(mc10_press_char(&m, MC10_KEY_ENTER, true) && m.keys[6] == 0x08);
    CHECK(mc10_press_char(&m, ' ', true) && m.keys[7] == 0x08);
    CHECK(mc10_press_char(&m, MC10_KEY_BREAK, true) && m.keys[2] == 0x40);
    CHECK(!mc10_press_char(&m, '~', true));                /* no such key */
    mc10_release_all(&m);
    CHECK(m.keys[2] == 0 && m.keys[6] == 0 && m.keys[7] == 0);
    mc10_key(&m, 9, 0, true);                              /* out of range is ignored */
    mc10_key(&m, 0, 8, true);
    CHECK(m.keys[0] == 0);
}

/* Author: Thomas Dzubin */
static void test_video_and_sound(void)
{
    int i, loud = 0;
    int16_t out[64];
    uint32_t got;

    /* the mode pins at 0xBFFF go to the video chip: 0x20 is AG, 0x7C all the others */
    fit_program("ORG $E000\n"
                " LDAA #$20\n STAA $BFFF\n"
                "hold: BRA hold\n");
    boot(MC10_RAM_4K);
    mc10_run(&m, 50);
    CHECK(m.vdg.mode == M6847_AG);
    rom_clear(0);
    fit_program("ORG $E000\n LDAA #$7C\n STAA $BFFF\nhold: BRA hold\n");
    boot(MC10_RAM_4K);
    mc10_run(&m, 50);
    CHECK(m.vdg.mode == (M6847_AG | M6847_GM0 | M6847_GM1 | M6847_GM2 | M6847_INTEXT | M6847_CSS));
    rom_clear(0);

    /* bit 7 is the sound output: a square wave of 4 + 4 cycles in a loop */
    fit_program("ORG $E000\n"
                " LDAA #$80\n LDAB #$00\n"
                "loop: STAA $BFFF\n STAB $BFFF\n BRA loop\n");
    boot(MC10_RAM_4K);
    mc10_run(&m, 2000);
    got = qaudio_read(&m.audio, out, 64);
    CHECK(got > 0);
    for (i = 0; i < (int)got; i++)
        if (out[i] > 100 || out[i] < -100)
            loud++;
    CHECK(loud > 0);
    rom_clear(0);

    /* with the sound bit never touched the output is silent */
    fit_program("ORG $E000\nhold: BRA hold\n");
    boot(MC10_RAM_4K);
    mc10_run(&m, 4000);
    got = qaudio_read(&m.audio, out, 64);
    for (i = 0; i < (int)got; i++)
        CHECK(out[i] > -100 && out[i] < 100);
    rom_clear(0);
}

/* Author: Thomas Dzubin */
static void test_screen_and_timing(void)
{
    int i, lit = 0;

    /* the program writes an A in the first cell; a frame later it is in the picture */
    fit_program("ORG $E000\n LDAA #$01\n STAA $4000\nhold: BRA hold\n");
    boot(MC10_RAM_4K);
    frame();
    CHECK(m.vdg.frames >= 1);
    mc6847_render_line(&m.vdg, 3, px);
    for (i = 0; i < 8; i++)
        lit += px[i] == 13;
    CHECK(lit == 3);                                        /* the top of the A: .###. */
    rom_clear(0);

    /* the video chip is kept to the processor's cycles: 14,934 cycles a frame */
    fit_program("ORG $E000\nhold: BRA hold\n");
    boot(MC10_RAM_4K);
    mc10_run(&m, 14934 * 5);
    CHECK(m.vdg.frames == 5 || m.vdg.frames == 4);
    CHECK(m.cpu.cycles >= 14934u * 5);
    rom_clear(0);
}

static void test_cassette(void)
{
    fit_program("ORG $E000\n"
                " LDAA $03\n STAA $4000\n"                 /* port 2 with the input high */
                " LDAA #$01\n STAA $01\n STAA $03\n"        /* port 2 bit 0 is an output, set */
                "hold: BRA hold\n");
    boot(MC10_RAM_4K);
    mc10_run(&m, 100);
    CHECK((mc10_peek(&m, 0x4000) & MC10_P2_CASSETTE_IN) != 0);
    CHECK(m.cassette_out);
    mc10_set_cassette_in(&m, false);
    rom_clear(0);
    fit_program("ORG $E000\n LDAA $03\n STAA $4000\nhold: BRA hold\n");
    boot(MC10_RAM_4K);
    mc10_set_cassette_in(&m, false);
    mc10_run(&m, 100);
    CHECK((mc10_peek(&m, 0x4000) & MC10_P2_CASSETTE_IN) == 0);
    rom_clear(0);
}

/* Author: Thomas Dzubin */
static void test_shift_is_shared(void)
{
    boot(MC10_RAM_4K);
    /* two shifted symbols down together: SHIFT stays until the last is let go */
    CHECK(mc10_press_char(&m, '!', true));
    CHECK(mc10_press_char(&m, '"', true));
    CHECK(mc10_press_char(&m, '!', false));
    CHECK(m.keys[1] == 0 && m.keys[2] == 0x10 && m.keys[7] == MC10_KEYLINE_BIT);
    CHECK(mc10_press_char(&m, '"', false));
    CHECK(m.keys[2] == 0 && m.keys[7] == 0);

    /* a SHIFT that is held on its own is not let go with a symbol */
    CHECK(mc10_press_char(&m, MC10_KEY_SHIFT, true));
    CHECK(mc10_press_char(&m, '!', true));
    CHECK(mc10_press_char(&m, '!', false));
    CHECK(m.keys[1] == 0 && m.keys[7] == MC10_KEYLINE_BIT);
    mc10_release_all(&m);
    CHECK(m.keys[7] == 0);

    /* the same key held as a plain character and as a symbol */
    CHECK(mc10_press_char(&m, '1', true));
    CHECK(mc10_press_char(&m, '!', true));
    CHECK(mc10_press_char(&m, '!', false));
    CHECK(m.keys[1] == 0x10 && m.keys[7] == 0);
    CHECK(mc10_press_char(&m, '1', false));
    CHECK(m.keys[1] == 0);

    /* a key pressed twice (auto-repeat) is one key */
    CHECK(mc10_press_char(&m, '!', true));
    CHECK(mc10_press_char(&m, '!', true));
    CHECK(mc10_press_char(&m, '!', false));
    CHECK(m.keys[1] == 0 && m.keys[7] == 0);

    /* the page of the message program is a whole page */
    CHECK(sizeof m.fallback == MEMBUS_PAGE_SIZE);
}

/* The sound for a stretch of cycles at once is the sound that the same cycles one
 * at a time make.
 *
 * Author: Thomas Dzubin */
static void test_audio_batches(void)
{
    static qaudio_t one, many;
    static int16_t a[QAUDIO_FIFO], b[QAUDIO_FIFO];
    uint32_t seed = 12345, i, n;
    bool level = false;
    int round, same = 1;

    qaudio_init(&one, MC10_CPU_HZ);
    qaudio_init(&many, MC10_CPU_HZ);
    for (round = 0; round < 3000; round++) {
        uint32_t count;

        seed = seed * 1103515245u + 12345u;
        count = (seed >> 16) % 40;
        if (((seed >> 8) & 3) == 0)
            level = !level;
        for (i = 0; i < count; i++)
            qaudio_cycle(&one, level);
        qaudio_cycles(&many, level, count);
        if (one.count != many.count || one.phase != many.phase || one.high != many.high ||
            one.length != many.length || one.dc != many.dc)
            same = 0;
    }
    CHECK(same);
    CHECK(one.count > 0);
    n = qaudio_read(&one, a, QAUDIO_FIFO);
    CHECK(qaudio_read(&many, b, QAUDIO_FIFO) == n);
    CHECK(memcmp(a, b, n * sizeof a[0]) == 0);
}

int main(void)
{
    test_no_rom();
    test_memory_map();
    test_keyboard();
    test_keys_by_character();
    test_shift_is_shared();
    test_audio_batches();
    test_video_and_sound();
    test_screen_and_timing();
    test_cassette();
    printf("%d checks, %d failed\n", checks, failures);
    return failures != 0;
}
