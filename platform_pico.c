/*
 * platform_pico.c - platform.h on the Raspberry Pi Pico SDK (PicoCalc
 * firmware, RP2040 and RP2350). Saved programs go to the SD card through the
 * vendored FAT32 driver; the sound is PWM on the speaker pins fed by DMA.
 *
 * Author: Thomas Dzubin
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "hardware/watchdog.h"

#include "sdcard.h"           /* SD_BLOCK_SIZE, which fat32.h needs */
#include "fat32.h"

#include "platform.h"
#include "platform_pico_const.h"

void plat_init(void)
{
    stdio_init_all();
    fat32_init();             /* sets up the SD card pins; no card access yet */
}

uint32_t plat_now_ms(void)
{
    return to_ms_since_boot(get_absolute_time());
}

uint32_t plat_now_us(void)
{
    return time_us_32();
}

void plat_sleep_ms(uint32_t ms)
{
    sleep_ms(ms);
}

/* ------------------------------------------------------------------ */
/*  Saved programs                                                      */
/* ------------------------------------------------------------------ */
static int file_result(fat32_error_t e)
{
    switch (e) {
    case FAT32_OK:
        return PLAT_FILE_OK;
    case FAT32_ERROR_NO_CARD:
    case FAT32_ERROR_INIT_FAILED:
    case FAT32_ERROR_NOT_MOUNTED:
    case FAT32_ERROR_INVALID_FORMAT:
        return PLAT_FILE_NO_CARD;
    case FAT32_ERROR_FILE_NOT_FOUND:
    case FAT32_ERROR_DIR_NOT_FOUND:
    case FAT32_ERROR_INVALID_PATH:
        return PLAT_FILE_NOT_FOUND;
    default:
        return PLAT_FILE_FAILED;
    }
}

static void program_path(char *path, size_t size, const char *name)
{
    snprintf(path, size, "/%s/%s", PLAT_PROGRAM_DIR, name);
}

/* Make sure the program folder exists. */
static int ensure_dir(void)
{
    fat32_file_t dir;
    char path[PLAT_PATH_MAX];
    fat32_error_t e;

    snprintf(path, sizeof path, "/%s", PLAT_PROGRAM_DIR);
    e = fat32_open(&dir, path);
    if (e == FAT32_OK) {
        fat32_close(&dir);
        return PLAT_FILE_OK;
    }
    e = fat32_dir_create(&dir, path);
    fat32_close(&dir);
    return file_result(e) == PLAT_FILE_OK ? PLAT_FILE_OK : PLAT_FILE_FAILED;
}

static fat32_file_t piece_file;                     /* the file being written in pieces */
static bool piece_open;

/* Author: Thomas Dzubin */
int plat_file_begin(const char *name)
{
    char path[PLAT_PATH_MAX];
    int rc;

    if (piece_open) {
        fat32_close(&piece_file);
        piece_open = false;
    }
    if (!fat32_is_ready())
        return PLAT_FILE_NO_CARD;
    rc = ensure_dir();
    if (rc != PLAT_FILE_OK)
        return rc;
    program_path(path, sizeof path, name);
    fat32_delete(path);                 /* replace; fine if it is not there */
    if (fat32_create(&piece_file, path) != FAT32_OK)
        return PLAT_FILE_FAILED;
    piece_open = true;
    return PLAT_FILE_OK;
}

int plat_file_write(const uint8_t *data, uint32_t len)
{
    size_t written = 0;
    fat32_error_t e;

    if (!piece_open)
        return PLAT_FILE_FAILED;
    e = fat32_write(&piece_file, data, len, &written);
    return (e == FAT32_OK && written == len) ? PLAT_FILE_OK : PLAT_FILE_FAILED;
}

int plat_file_end(void)
{
    fat32_error_t e;

    if (!piece_open)
        return PLAT_FILE_FAILED;
    e = fat32_close(&piece_file);
    piece_open = false;
    return e == FAT32_OK ? PLAT_FILE_OK : PLAT_FILE_FAILED;
}

/* Author: Thomas Dzubin */
int plat_file_save(const char *name, const uint8_t *data, uint32_t len)
{
    fat32_file_t f;
    char path[PLAT_PATH_MAX];
    size_t written = 0;
    fat32_error_t e;
    int rc;

    if (!fat32_is_ready())
        return PLAT_FILE_NO_CARD;
    rc = ensure_dir();
    if (rc != PLAT_FILE_OK)
        return rc;

    program_path(path, sizeof path, name);
    fat32_delete(path);                 /* replace; fine if it is not there */
    e = fat32_create(&f, path);
    if (e != FAT32_OK)
        return PLAT_FILE_FAILED;
    e = fat32_write(&f, data, len, &written);
    fat32_close(&f);
    return (e == FAT32_OK && written == len) ? PLAT_FILE_OK : PLAT_FILE_FAILED;
}

/* Author: Thomas Dzubin */
int plat_file_load(const char *name, uint8_t *data, uint32_t max, uint32_t *len)
{
    fat32_file_t f;
    char path[PLAT_PATH_MAX];
    size_t got = 0;
    fat32_error_t e;

    *len = 0;
    if (!fat32_is_ready())
        return PLAT_FILE_NO_CARD;

    program_path(path, sizeof path, name);
    e = fat32_open(&f, path);
    if (e != FAT32_OK)
        return file_result(e);
    e = fat32_read(&f, data, max, &got);
    fat32_close(&f);
    if (e != FAT32_OK)
        return PLAT_FILE_FAILED;
    *len = (uint32_t)got;
    return PLAT_FILE_OK;
}

/* True for a name that ends in the given extension, whatever the case. */
static bool has_ext(const char *name, const char *ext)
{
    size_t n = strlen(name);
    size_t e = strlen(ext);

    return n > e && strcasecmp(name + n - e, ext) == 0;
}

/* True for a file of the kind asked for: a program (a raw image, an Intel HEX
 * file or an assembly source) or just an assembly source. */
static bool has_kind_ext(const char *name, int kind)
{
    if (kind == PLAT_KIND_SOURCE)
        return has_ext(name, PLAT_SOURCE_EXT);
    if (kind == PLAT_KIND_ROM)
        return has_ext(name, PLAT_ROM_EXT);
    return has_ext(name, PLAT_PROGRAM_EXT) || has_ext(name, PLAT_HEX_EXT) ||
           has_ext(name, PLAT_SOURCE_EXT);
}

/* Author: Thomas Dzubin */
int plat_file_list(char names[][PLAT_NAME_MAX], int max, int *count, int kind)
{
    fat32_file_t dir;
    fat32_entry_t entry;
    char path[PLAT_PATH_MAX];
    fat32_error_t e;
    int n = 0;
    int i, j;

    *count = 0;
    if (!fat32_is_ready())
        return PLAT_FILE_NO_CARD;

    snprintf(path, sizeof path, "/%s", PLAT_PROGRAM_DIR);
    e = fat32_open(&dir, path);
    if (e != FAT32_OK)
        return file_result(e);          /* no folder yet: not found */

    while (n < max && fat32_dir_read(&dir, &entry) == FAT32_OK &&
           entry.filename[0]) {
        if (entry.attr & (FAT32_ATTR_DIRECTORY | FAT32_ATTR_VOLUME_ID |
                          FAT32_ATTR_HIDDEN | FAT32_ATTR_SYSTEM))
            continue;
        if (strlen(entry.filename) >= PLAT_NAME_MAX ||
            !has_kind_ext(entry.filename, kind))
            continue;
        strcpy(names[n++], entry.filename);
    }
    fat32_close(&dir);

    for (i = 1; i < n; i++) {           /* insertion sort, a few names */
        char tmp[PLAT_NAME_MAX];

        strcpy(tmp, names[i]);
        for (j = i - 1; j >= 0 && strcasecmp(names[j], tmp) > 0; j--)
            strcpy(names[j + 1], names[j]);
        strcpy(names[j + 1], tmp);
    }
    *count = n;
    return PLAT_FILE_OK;
}

/* ------------------------------------------------------------------ */
/*  Sound: PWM on the speaker pins, fed from a ring buffer by DMA       */
/* ------------------------------------------------------------------ */
/* The ring is as long as its own size in bytes is aligned, which is what the
 * DMA channel's address wrap needs. */
static uint32_t audio_ring[AUDIO_RING_SIZE]
    __attribute__((aligned(AUDIO_RING_SIZE * sizeof(uint32_t))));
static int      audio_channel = -1;
static uint32_t audio_wrote;        /* samples put in the ring, ever (wraps)  */
static uint32_t audio_played;       /* samples the DMA has taken, ever (wraps) */
static uint32_t audio_index;        /* where in the ring the DMA was last seen */
static bool     audio_primed;       /* the cushion of silence has been laid   */

/* One sample as the value for the PWM slice's level register, which holds the
 * left channel's level in its low half and the right one's in its high half. */
static uint32_t audio_level(int16_t sample)
{
    uint32_t level = (uint32_t)((int32_t)sample + AUDIO_LEVEL_OFFSET) >> AUDIO_LEVEL_SHIFT;

    return level | (level << 16);
}

static void audio_fill_silence(void)
{
    uint32_t i;
    uint32_t silence = audio_level(0);

    for (i = 0; i < AUDIO_RING_SIZE; i++)
        audio_ring[i] = silence;
}

/* Note how far the DMA has read since the last look. This has to be done more
 * often than the ring takes to play, which the main loop does many times over. */
static void audio_look(void)
{
    uint32_t index = ((dma_hw->ch[audio_channel].read_addr -
                       (uint32_t)(uintptr_t)audio_ring) / sizeof(uint32_t)) &
                     AUDIO_RING_MASK;

    audio_played += (index - audio_index) & AUDIO_RING_MASK;
    audio_index = index;
}

/* The fraction num/den (each of 16 bits, which is what a DMA timer takes) that
 * puts the timer closest to the sample rate: it ticks at clk * num / den. For
 * each numerator the best denominator is the rounded quotient, and the pair
 * with the smallest error wins. Dividing clk by the rate and rounding to a
 * whole number would make the speaker play about 0.01 percent too fast, and the
 * few samples a second that adds up to would empty the cushion every few
 * minutes.
 *
 * Author: Thomas Dzubin */
static void audio_timer_fraction(uint32_t clk, uint32_t rate,
                                 uint16_t *num, uint16_t *den)
{
    uint64_t best_error = 0;
    uint32_t best_num = 0, best_den = 1;
    uint32_t n;

    for (n = 1; n <= 0xFFFFu; n++) {
        uint64_t d = ((uint64_t)n * clk + rate / 2u) / rate;
        uint64_t exact = (uint64_t)n * clk;
        uint64_t error;

        if (d == 0)
            continue;
        if (d > 0xFFFFu)
            break;
        error = exact > d * rate ? exact - d * rate : d * rate - exact;
        /* the relative error is error / (d * rate); compare without dividing */
        if (best_num == 0 || error * best_den < best_error * d) {
            best_error = error;
            best_num = n;
            best_den = (uint32_t)d;
        }
    }
    *num = (uint16_t)best_num;
    *den = (uint16_t)best_den;
}

/* Author: Thomas Dzubin */
void plat_audio_init(void)
{
    uint slice;
    uint timer;
    uint16_t timer_num, timer_den;
    pwm_config pwm = pwm_get_default_config();
    dma_channel_config dma;

    gpio_set_function(AUDIO_PIN_LEFT, GPIO_FUNC_PWM);
    gpio_set_function(AUDIO_PIN_RIGHT, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(AUDIO_PIN_LEFT);
    pwm_config_set_wrap(&pwm, AUDIO_PWM_WRAP);
    pwm_init(slice, &pwm, true);

    audio_fill_silence();
    timer = (uint)dma_claim_unused_timer(true);
    audio_timer_fraction(clock_get_hz(clk_sys), PLAT_AUDIO_RATE,
                         &timer_num, &timer_den);
    dma_timer_set_fraction(timer, timer_num, timer_den);
    audio_channel = dma_claim_unused_channel(true);
    dma = dma_channel_get_default_config((uint)audio_channel);
    channel_config_set_transfer_data_size(&dma, DMA_SIZE_32);
    channel_config_set_read_increment(&dma, true);
    channel_config_set_write_increment(&dma, false);
    channel_config_set_ring(&dma, false, AUDIO_RING_BITS + 2);
    channel_config_set_dreq(&dma, dma_get_timer_dreq(timer));
    dma_channel_configure((uint)audio_channel, &dma, &pwm_hw->slice[slice].cc,
                          audio_ring, AUDIO_DMA_COUNT, true);
}

/* Author: Thomas Dzubin */
void plat_audio_play(const int16_t *samples, uint32_t count)
{
    int32_t fill;
    uint32_t room, i;

    if (audio_channel < 0 || count == 0)
        return;
    if (!dma_channel_is_busy((uint)audio_channel)) {    /* the long count ran out */
        dma_channel_set_trans_count((uint)audio_channel, AUDIO_DMA_COUNT, true);
        audio_primed = false;
    }
    audio_look();

    fill = (int32_t)(audio_wrote - audio_played);
    if (!audio_primed || fill < 0) {
        /* The start, or the DMA has overtaken what was written (it has been
         * playing old samples): lay a short stretch of silence ahead of it
         * and carry on from the end of that. */
        for (i = 0; i < PLAT_AUDIO_PRIME; i++)
            audio_ring[(audio_index + i) & AUDIO_RING_MASK] = audio_level(0);
        audio_wrote = audio_played + PLAT_AUDIO_PRIME;
        audio_primed = true;
        fill = PLAT_AUDIO_PRIME;
    }

    room = fill < PLAT_AUDIO_MAX_FILL ? (uint32_t)(PLAT_AUDIO_MAX_FILL - fill) : 0;
    if (count > room)
        count = room;                   /* the rest is dropped */
    for (i = 0; i < count; i++)
        audio_ring[(audio_wrote + i) & AUDIO_RING_MASK] = audio_level(samples[i]);
    audio_wrote += count;
}

void plat_audio_stop(void)
{
    if (audio_channel < 0)
        return;
    audio_fill_silence();
    audio_look();
    audio_wrote = audio_played;
    audio_primed = false;
}

/* ------------------------------------------------------------------ */
/*  Leaving the program                                                 */
/* ------------------------------------------------------------------ */
_Noreturn void plat_bootsel(void)
{
    reset_usb_boot(0, 0);
    for (;;)
        tight_loop_contents();
}

_Noreturn void plat_exit_to_loader(void)
{
    watchdog_hw->scratch[LOADER_SCRATCH_MODE] = LOADER_BOOT_MODE_SD;
    watchdog_hw->scratch[LOADER_SCRATCH_ARGUMENT] = 0;
    watchdog_hw->scratch[LOADER_SCRATCH_MAGIC] = LOADER_COMMAND_MAGIC;
    watchdog_reboot(0, 0, LOADER_REBOOT_DELAY_MS);
    for (;;)
        tight_loop_contents();      /* the reboot comes in a few ms */
}
