/*
 * platform.h - the few things elf1802.c needs from the machine and the
 * operating system besides the LCD and keyboard drivers: starting the
 * runtime, clocks, sleeping, sound, saved programs, and the two ways of leaving
 * the program. Each platform supplies its own implementation:
 *
 *     PicoCalc firmware   platform_pico.c
 *     Windows / Linux     desktop/platform_desktop.c
 *
 * Author: Thomas Dzubin
 */
#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>

#include "platform_const.h"

void     plat_init(void);               /* start the runtime, once at startup  */
uint32_t plat_now_ms(void);             /* milliseconds since start (wraps)    */
uint32_t plat_now_us(void);             /* microseconds since start (wraps)    */
void     plat_sleep_ms(uint32_t ms);    /* wait; the platform keeps running    */

/* Saved programs: raw memory images in the PLAT_PROGRAM_DIR folder, named in
 * 8.3 style. They return PLAT_FILE_OK or one of the other PLAT_FILE_* codes.
 *
 *   plat_file_save   writes len bytes, replacing any file of that name
 *   plat_file_load   reads up to max bytes; *len is how many were read
 *   plat_file_list   fills names with the files found of the given kind, in
 *                    alphabetical order, at most max of them: PLAT_KIND_PROGRAM
 *                    lists those that end in PLAT_PROGRAM_EXT, PLAT_HEX_EXT or
 *                    PLAT_SOURCE_EXT, PLAT_KIND_SOURCE only those that end in
 *                    PLAT_SOURCE_EXT, PLAT_KIND_ROM those that end in
 *                    PLAT_ROM_EXT;
 *                    *count is how many were stored
 */
int plat_file_save(const char *name, const uint8_t *data, uint32_t len);
/* A file written in pieces (for a long listing): begin creates (or replaces) the
 * file, write adds to it, end closes it; only one at a time. */
int plat_file_begin(const char *name);
int plat_file_write(const uint8_t *data, uint32_t len);
int plat_file_end(void);
int plat_file_load(const char *name, uint8_t *data, uint32_t max, uint32_t *len);
int plat_file_list(char names[][PLAT_NAME_MAX], int max, int *count, int kind);

/* Sound: signed 16-bit mono samples at PLAT_AUDIO_RATE, played in order as
 * they arrive. plat_audio_init() once at start-up; plat_audio_play() queues
 * samples (any that do not fit the short queue are dropped, and a queue that
 * has run dry is refilled with a little silence first); plat_audio_stop()
 * silences the output and forgets whatever is queued. */
void plat_audio_init(void);
void plat_audio_play(const int16_t *samples, uint32_t count);
void plat_audio_stop(void);

/* Leave the program. Neither one returns. On the PicoCalc, plat_bootsel()
 * opens the USB drive (BOOTSEL) mode and plat_exit_to_loader() goes back to
 * the UF2 Loader menu; on a PC both just exit. */
_Noreturn void plat_bootsel(void);
_Noreturn void plat_exit_to_loader(void);

#endif /* PLATFORM_H */
