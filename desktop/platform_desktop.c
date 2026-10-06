/*
 * platform_desktop.c - platform.h for the Windows / Linux (SDL2) build. The
 * clock and sleep come from the shim's core; there is no BOOTSEL or UF2 Loader
 * on a PC, so both just close the program. Saved programs are plain files in a
 * folder beside the program (in the folder it is run from).
 *
 * Author: Thomas Dzubin
 */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define MAKE_DIR(path)  _mkdir(path)
#define strcasecmp      _stricmp
#else
#include <strings.h>
#define MAKE_DIR(path)  mkdir((path), 0777)
#endif

#include <SDL.h>                      /* the sound goes straight to SDL */

#include "pico/stdlib.h"            /* the shim's stand-in header */

#include "platform.h"

void plat_init(void)
{
    stdio_init_all();
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
static void program_path(char *path, size_t size, const char *name)
{
    snprintf(path, size, "%s/%s", PLAT_PROGRAM_DIR, name);
}

static FILE *piece_file;                            /* the file being written in pieces */

int plat_file_begin(const char *name)
{
    char path[PLAT_PATH_MAX];

    if (piece_file) {
        fclose(piece_file);
        piece_file = NULL;
    }
    MAKE_DIR(PLAT_PROGRAM_DIR);         /* fine if it already exists */
    program_path(path, sizeof path, name);
    piece_file = fopen(path, "wb");
    return piece_file ? PLAT_FILE_OK : PLAT_FILE_FAILED;
}

int plat_file_write(const uint8_t *data, uint32_t len)
{
    if (!piece_file)
        return PLAT_FILE_FAILED;
    return fwrite(data, 1, len, piece_file) == len ? PLAT_FILE_OK : PLAT_FILE_FAILED;
}

int plat_file_end(void)
{
    int rc;

    if (!piece_file)
        return PLAT_FILE_FAILED;
    rc = fclose(piece_file);
    piece_file = NULL;
    return rc == 0 ? PLAT_FILE_OK : PLAT_FILE_FAILED;
}

int plat_file_save(const char *name, const uint8_t *data, uint32_t len)
{
    char path[PLAT_PATH_MAX];
    FILE *f;
    size_t written;

    MAKE_DIR(PLAT_PROGRAM_DIR);         /* fine if it already exists */
    program_path(path, sizeof path, name);
    f = fopen(path, "wb");
    if (!f)
        return PLAT_FILE_FAILED;
    written = fwrite(data, 1, len, f);
    if (fclose(f) != 0 || written != len)
        return PLAT_FILE_FAILED;
    return PLAT_FILE_OK;
}

int plat_file_load(const char *name, uint8_t *data, uint32_t max, uint32_t *len)
{
    char path[PLAT_PATH_MAX];
    FILE *f;

    *len = 0;
    program_path(path, sizeof path, name);
    f = fopen(path, "rb");
    if (!f)
        return PLAT_FILE_NOT_FOUND;
    *len = (uint32_t)fread(data, 1, max, f);
    fclose(f);
    return PLAT_FILE_OK;
}

static int has_ext(const char *name, const char *ext)
{
    size_t n = strlen(name);
    size_t e = strlen(ext);

    return n > e && strcasecmp(name + n - e, ext) == 0;
}

static int has_kind_ext(const char *name, int kind)
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
    DIR *dir;
    struct dirent *ent;
    int n = 0;
    int i, j;

    *count = 0;
    dir = opendir(PLAT_PROGRAM_DIR);
    if (!dir)
        return PLAT_FILE_NOT_FOUND;

    while (n < max && (ent = readdir(dir)) != NULL) {
        if (strlen(ent->d_name) >= PLAT_NAME_MAX ||
            !has_kind_ext(ent->d_name, kind))
            continue;
        strcpy(names[n++], ent->d_name);
    }
    closedir(dir);

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
/*  Sound: SDL's audio queue, one channel of 16-bit samples             */
/* ------------------------------------------------------------------ */
#define AUDIO_DEVICE_SAMPLES    512         /* SDL's own buffer, in samples */

static SDL_AudioDeviceID audio_device;

void plat_audio_init(void)
{
    SDL_AudioSpec want;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
        return;                             /* no sound, carry on silently */
    SDL_zero(want);
    want.freq = PLAT_AUDIO_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = AUDIO_DEVICE_SAMPLES;
    audio_device = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
    if (audio_device)
        SDL_PauseAudioDevice(audio_device, 0);
}

void plat_audio_play(const int16_t *samples, uint32_t count)
{
    uint32_t queued, room;

    if (!audio_device || count == 0)
        return;
    queued = SDL_GetQueuedAudioSize(audio_device) / sizeof(int16_t);
    if (queued < PLAT_AUDIO_PRIME / 4) {    /* running dry: lay down a cushion */
        int16_t silence[PLAT_AUDIO_PRIME] = { 0 };

        SDL_QueueAudio(audio_device, silence,
                       (PLAT_AUDIO_PRIME - queued) * (uint32_t)sizeof(int16_t));
        queued = PLAT_AUDIO_PRIME;
    }
    room = queued < PLAT_AUDIO_MAX_FILL ? PLAT_AUDIO_MAX_FILL - queued : 0;
    if (count > room)
        count = room;                       /* the rest is dropped */
    if (count > 0)
        SDL_QueueAudio(audio_device, samples, count * (uint32_t)sizeof(int16_t));
}

void plat_audio_stop(void)
{
    if (audio_device)
        SDL_ClearQueuedAudio(audio_device);
}

/* ------------------------------------------------------------------ */
/*  Leaving the program                                                 */
/* ------------------------------------------------------------------ */
_Noreturn void plat_bootsel(void)
{
    exit(0);
}

_Noreturn void plat_exit_to_loader(void)
{
    exit(0);
}
