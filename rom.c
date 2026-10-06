/*
 * rom.c - ROM images. See rom.h.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "platform.h"
#include "rom.h"

static rom_info_t slots[ROM_SLOTS];
static uint8_t    images[ROM_SLOTS][ROM_SLOT_SIZE];

/* Author: Thomas Dzubin */
int rom_set(int slot, const char *name, uint32_t address)
{
    uint32_t len = 0;
    uint32_t size;
    int rc;

    if (slot < 0 || slot >= ROM_SLOTS)
        return ROM_ERR_SLOT;
    rom_clear(slot);                    /* (the file is read straight into the slot) */
    if (address & (MEMBUS_PAGE_SIZE - 1u))
        return ROM_ERR_ADDRESS;
    memset(images[slot], 0, sizeof images[slot]);
    rc = plat_file_load(name, images[slot], ROM_SLOT_SIZE, &len);
    if (rc != PLAT_FILE_OK)
        return rc;
    if (len == 0)
        return ROM_ERR_EMPTY;
    size = (len + MEMBUS_PAGE_SIZE - 1u) & ~(MEMBUS_PAGE_SIZE - 1u);
    if (address + size > MEMBUS_SPACE)
        return ROM_ERR_ADDRESS;
    slots[slot].fitted = true;
    snprintf(slots[slot].name, sizeof slots[slot].name, "%s", name);
    slots[slot].address = address;
    slots[slot].size = size;
    return ROM_OK;
}

void rom_clear(int slot)
{
    if (slot >= 0 && slot < ROM_SLOTS)
        memset(&slots[slot], 0, sizeof slots[slot]);
}

const rom_info_t *rom_info(int slot)
{
    return slot >= 0 && slot < ROM_SLOTS ? &slots[slot] : NULL;
}

void rom_apply(membus_t *bus)
{
    int i;

    for (i = 0; i < ROM_SLOTS; i++)
        if (slots[i].fitted)
            membus_map_rom(bus, slots[i].address, slots[i].size, images[i]);
}

/* ------------------------------------------------------------------ */
/*  Remembering what is fitted                                          */
/* ------------------------------------------------------------------ */
void rom_config_save(void)
{
    char text[ROM_SLOTS * 32];
    size_t pos = 0;
    int i;

    for (i = 0; i < ROM_SLOTS; i++)
        if (slots[i].fitted)
            pos += (size_t)snprintf(text + pos, sizeof text - pos, "%d %s %04X\n", i + 1,
                                    slots[i].name, (unsigned)slots[i].address);
    plat_file_save(ROM_CONFIG_FILE, (const uint8_t *)text, (uint32_t)pos);
}

/* A hex number at *pp (digits only), moving past it; false if there is none. */
static bool hex_number(const char **pp, unsigned *value)
{
    const char *p = *pp;
    unsigned v = 0;
    int digits = 0;

    for (; (*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f'); p++, digits++)
        v = (v << 4) | (unsigned)(*p <= '9' ? *p - '0' : (*p | 0x20) - 'a' + 10);
    if (digits == 0 || digits > 8)
        return false;
    *value = v;
    *pp = p;
    return true;
}

/* Fit what the config file says: lines of "slot name address", the address in hex.
 * (Read by hand: sscanf would bring in a lot of library for three fields.)
 *
 * Author: Thomas Dzubin */
void rom_config_load(void)
{
    static uint8_t text[ROM_SLOTS * 32 + 1];
    uint32_t len = 0;
    char *p;

    if (plat_file_load(ROM_CONFIG_FILE, text, sizeof text - 1, &len) != PLAT_FILE_OK)
        return;
    text[len] = '\0';
    for (p = (char *)text; *p; ) {
        const char *line = p;
        char name[ROM_NAME_MAX];
        unsigned slot = 0, address = 0;
        size_t n = 0;

        while (*p && *p != '\n')
            p++;
        if (*p)
            *p++ = '\0';
        if (!hex_number(&line, &slot) || *line != ' ')
            continue;
        while (*line == ' ')
            line++;
        while (*line && *line != ' ' && n + 1 < sizeof name)
            name[n++] = *line++;
        name[n] = '\0';
        while (*line == ' ')
            line++;
        if (n > 0 && hex_number(&line, &address) && slot >= 1 && slot <= ROM_SLOTS)
            rom_set((int)slot - 1, name, address);
    }
}
