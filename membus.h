/*
 * membus.h - the memory map of a machine: 64K of address space divided into
 * pages of 256 bytes, each page being RAM, ROM, or a device (a pair of
 * functions), or nothing. Any CPU with a 16-bit address bus uses it the same
 * way; the board maps what it has and the CPU core reads and writes through
 * membus_read() and membus_write().
 *
 * RAM and ROM are read and written directly through a pointer, which keeps
 * the common case fast. RAM can be write protected and unprotected again,
 * page by page (a memory protect switch, a ROM that shadows RAM). A page that
 * is nothing reads as the open bus value and ignores writes.
 *
 * Portable ISO C.
 *
 * Author: Thomas Dzubin
 */
#ifndef MEMBUS_H
#define MEMBUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "membus_const.h"

/* A device on a page: it is given the full 16-bit address. */
typedef uint8_t (*membus_read_fn)(void *ctx, uint16_t addr);
typedef void    (*membus_write_fn)(void *ctx, uint16_t addr, uint8_t data);

typedef struct {
    uint8_t        *read_ptr;       /* reads come straight from here, or NULL  */
    uint8_t        *write_ptr;      /* writes go straight here, or NULL        */
    uint8_t        *storage;        /* the RAM behind the page, for protection */
    membus_read_fn  read_fn;        /* used when read_ptr is NULL              */
    membus_write_fn write_fn;       /* used when write_ptr is NULL             */
    void           *ctx;
} membus_page_t;

typedef struct {
    membus_page_t page[MEMBUS_PAGES];
    uint8_t       open_bus;         /* what a read of nothing gives            */
} membus_t;

/* Nothing mapped anywhere; reads of nothing give open_bus. */
void membus_init(membus_t *bus, uint8_t open_bus);

/* Map size bytes at addr (both multiples of the page size; anything outside the
 * address space is left out). storage must be size bytes long. */
void membus_map_ram(membus_t *bus, uint32_t addr, uint32_t size, uint8_t *storage);
void membus_map_rom(membus_t *bus, uint32_t addr, uint32_t size, uint8_t *storage);
void membus_map_io(membus_t *bus, uint32_t addr, uint32_t size,
                   membus_read_fn read_fn, membus_write_fn write_fn, void *ctx);
/* Memory that is read directly but whose writes go to a function (which stores
 * the byte itself, and can count them or do something else as well). */
void membus_map_hooked(membus_t *bus, uint32_t addr, uint32_t size, uint8_t *storage,
                       membus_write_fn write_fn, void *ctx);
void membus_unmap(membus_t *bus, uint32_t addr, uint32_t size);

/* Write protect (or unprotect) RAM mapped with membus_map_ram(). Other pages are
 * left as they are. */
void membus_write_protect(membus_t *bus, uint32_t addr, uint32_t size, bool protect);

static inline uint8_t membus_read(const membus_t *bus, uint16_t addr)
{
    const membus_page_t *p = &bus->page[addr >> MEMBUS_PAGE_SHIFT];

    if (p->read_ptr)
        return p->read_ptr[addr & (MEMBUS_PAGE_SIZE - 1u)];
    if (p->read_fn)
        return p->read_fn(p->ctx, addr);
    return bus->open_bus;
}

static inline void membus_write(const membus_t *bus, uint16_t addr, uint8_t data)
{
    const membus_page_t *p = &bus->page[addr >> MEMBUS_PAGE_SHIFT];

    if (p->write_ptr)
        p->write_ptr[addr & (MEMBUS_PAGE_SIZE - 1u)] = data;
    else if (p->write_fn)
        p->write_fn(p->ctx, addr, data);
}

/* A read with no side effect, for displays of memory: devices that are read by
 * functions give the open bus value. */
static inline uint8_t membus_peek(const membus_t *bus, uint16_t addr)
{
    const membus_page_t *p = &bus->page[addr >> MEMBUS_PAGE_SHIFT];

    return p->read_ptr ? p->read_ptr[addr & (MEMBUS_PAGE_SIZE - 1u)] : bus->open_bus;
}

#endif /* MEMBUS_H */
