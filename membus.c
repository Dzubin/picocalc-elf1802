/*
 * membus.c - the memory map. See membus.h.
 *
 * Author: Thomas Dzubin
 */
#include <string.h>

#include "membus.h"

void membus_init(membus_t *bus, uint8_t open_bus)
{
    memset(bus, 0, sizeof *bus);
    bus->open_bus = open_bus;
}

/* The first page and the number of pages that a range covers, kept inside the
 * address space. Returns false if there are none. */
static bool range(uint32_t addr, uint32_t size, uint32_t *first, uint32_t *count)
{
    uint32_t last;

    if (addr >= MEMBUS_SPACE || size == 0)
        return false;
    if (addr + size > MEMBUS_SPACE)
        size = MEMBUS_SPACE - addr;
    *first = addr >> MEMBUS_PAGE_SHIFT;
    last = (addr + size - 1u) >> MEMBUS_PAGE_SHIFT;
    *count = last - *first + 1u;
    return true;
}

void membus_map_ram(membus_t *bus, uint32_t addr, uint32_t size, uint8_t *storage)
{
    uint32_t first, count, i;

    if (!range(addr, size, &first, &count))
        return;
    for (i = 0; i < count; i++) {
        membus_page_t *p = &bus->page[first + i];
        uint8_t *bytes = storage + i * MEMBUS_PAGE_SIZE;

        memset(p, 0, sizeof *p);
        p->read_ptr = bytes;
        p->write_ptr = bytes;
        p->storage = bytes;
    }
}

void membus_map_rom(membus_t *bus, uint32_t addr, uint32_t size, uint8_t *storage)
{
    uint32_t first, count, i;

    if (!range(addr, size, &first, &count))
        return;
    for (i = 0; i < count; i++) {
        membus_page_t *p = &bus->page[first + i];

        memset(p, 0, sizeof *p);
        p->read_ptr = storage + i * MEMBUS_PAGE_SIZE;
    }
}

void membus_map_io(membus_t *bus, uint32_t addr, uint32_t size,
                   membus_read_fn read_fn, membus_write_fn write_fn, void *ctx)
{
    uint32_t first, count, i;

    if (!range(addr, size, &first, &count))
        return;
    for (i = 0; i < count; i++) {
        membus_page_t *p = &bus->page[first + i];

        memset(p, 0, sizeof *p);
        p->read_fn = read_fn;
        p->write_fn = write_fn;
        p->ctx = ctx;
    }
}

void membus_map_hooked(membus_t *bus, uint32_t addr, uint32_t size, uint8_t *storage,
                       membus_write_fn write_fn, void *ctx)
{
    uint32_t first, count, i;

    if (!range(addr, size, &first, &count))
        return;
    for (i = 0; i < count; i++) {
        membus_page_t *p = &bus->page[first + i];

        memset(p, 0, sizeof *p);
        p->read_ptr = storage + i * MEMBUS_PAGE_SIZE;
        p->write_fn = write_fn;
        p->ctx = ctx;
    }
}

void membus_unmap(membus_t *bus, uint32_t addr, uint32_t size)
{
    uint32_t first, count, i;

    if (!range(addr, size, &first, &count))
        return;
    for (i = 0; i < count; i++)
        memset(&bus->page[first + i], 0, sizeof bus->page[0]);
}

void membus_write_protect(membus_t *bus, uint32_t addr, uint32_t size, bool protect)
{
    uint32_t first, count, i;

    if (!range(addr, size, &first, &count))
        return;
    for (i = 0; i < count; i++) {
        membus_page_t *p = &bus->page[first + i];

        if (p->storage)                 /* RAM from membus_map_ram() */
            p->write_ptr = protect ? NULL : p->storage;
    }
}
