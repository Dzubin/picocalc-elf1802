/*
 * rom.h - ROM images for any machine: up to ROM_SLOTS files from the SD card, each
 * fitted at an address in the machine's memory map as read-only memory over
 * whatever is there. Which images are fitted and where is remembered in a small
 * file (ROMS.CFG) and fitted again at power-on. The machine maps them on its
 * memory bus with rom_apply() when it is set up, and the file menu changes them.
 *
 * Portable ISO C except for reading the files, which goes through platform.h.
 *
 * Author: Thomas Dzubin
 */
#ifndef ROM_H
#define ROM_H

#include <stdbool.h>
#include <stdint.h>

#include "membus.h"
#include "rom_const.h"

typedef struct {
    bool     fitted;
    char     name[ROM_NAME_MAX];    /* the file                                  */
    uint32_t address;               /* where it is, a multiple of the page size  */
    uint32_t size;                  /* bytes, rounded up to whole pages          */
} rom_info_t;

/* Read a ROM image from the file name (in the program folder of the SD card) into
 * slot (0 to ROM_SLOTS - 1) and fit it at address, which must be a multiple of the
 * page size (256) with the whole image below 64K. Returns ROM_OK or an error (a
 * ROM_ERR_* or a PLAT_FILE_* result); on an error the slot is left empty. */
int rom_set(int slot, const char *name, uint32_t address);

/* Take a ROM out. */
void rom_clear(int slot);

/* What is in a slot (NULL for no such slot). */
const rom_info_t *rom_info(int slot);

/* Map the fitted images on a memory bus (as read-only memory, over whatever was
 * mapped there). Call it after the machine has mapped its own memory. */
void rom_apply(membus_t *bus);

/* Remember what is fitted, and fit what was remembered. */
void rom_config_save(void);
void rom_config_load(void);

#endif /* ROM_H */
