/*
 * mcard.c - the 1802 Membership Card. See mcard.h.
 *
 * Author: Thomas Dzubin
 */
#include <string.h>

#include "isa.h"
#include "mcard.h"
#include "rom.h"

/* ------------------------------------------------------------------ */
/*  The bus                                                             */
/* ------------------------------------------------------------------ */
static uint8_t bus_mem_read(void *ctx, uint16_t addr)
{
    const mcard_t *m = ctx;

    return membus_read(&m->bus, addr);
}

static void bus_mem_write(void *ctx, uint16_t addr, uint8_t data)
{
    const mcard_t *m = ctx;

    membus_write(&m->bus, addr, data);
}

static uint8_t bus_io_in(void *ctx, uint8_t port)
{
    const mcard_t *m = ctx;

    return port == MCARD_PORT_DATA ? m->data_switches : MCARD_UNMAPPED_READ;
}

static void bus_io_out(void *ctx, uint8_t port, uint8_t data)
{
    mcard_t *m = ctx;

    if (port == MCARD_PORT_DATA)
        m->display = data;
}

/* DMA-IN is the IN button in LOAD mode. With writing enabled the data switches
 * go to memory; with it disabled (READ) the memory ignores the write and the
 * byte at R0 is shown instead. */
static uint8_t bus_dma_in(void *ctx)
{
    mcard_t *m = ctx;
    uint8_t value;

    m->in_pending = false;
    value = m->sw_write ? m->data_switches : membus_peek(&m->bus, m->cpu.r[0]);
    m->display = value;
    return value;
}

static void bus_dma_out(void *ctx, uint8_t data)
{
    (void)ctx;
    (void)data;                         /* nothing is on the DMA-OUT line */
}

/* ------------------------------------------------------------------ */
/*  Set-up                                                              */
/* ------------------------------------------------------------------ */
void mcard_map_memory(mcard_t *m)
{
    membus_init(&m->bus, MCARD_UNMAPPED_READ);
    membus_map_ram(&m->bus, 0, MCARD_RAM_SIZE, m->ram);
    if (m->rom_length > 0)
        membus_map_rom(&m->bus, MCARD_ROM_BASE, MCARD_MONITOR_MAX, m->rom);
    rom_apply(&m->bus);                 /* the ROM images from the card go over it */
    membus_write_protect(&m->bus, 0, MCARD_RAM_SIZE, !m->sw_write);
}

void mcard_init(mcard_t *m, const uint8_t *monitor, uint32_t monitor_length)
{
    memset(m, 0, sizeof *m);
    if (monitor && monitor_length > 0) {
        if (monitor_length > MCARD_MONITOR_MAX)
            monitor_length = MCARD_MONITOR_MAX;
        memcpy(m->rom, monitor, monitor_length);
        m->rom_length = monitor_length;
    }
    m->sw_wait_up = true;
    m->sw_clr_up = false;               /* the card starts cleared */
    m->sw_write = true;
    m->was_clear = true;
    serial_init(&m->serial);
    serial_set_speed(&m->serial, MCARD_DEFAULT_BAUD, MCARD_CYCLES_PER_SEC);
    cpu1802_power_on(&m->cpu);
    m->cpu.ctx = m;
    m->cpu.mem_read = bus_mem_read;
    m->cpu.mem_write = bus_mem_write;
    m->cpu.io_in = bus_io_in;
    m->cpu.io_out = bus_io_out;
    m->cpu.dma_in = bus_dma_in;
    m->cpu.dma_out = bus_dma_out;
    mcard_map_memory(m);
}

/* ------------------------------------------------------------------ */
/*  The front panel                                                     */
/* ------------------------------------------------------------------ */
void mcard_set_switches(mcard_t *m, bool wait_up, bool clr_up, bool write_enable)
{
    m->sw_wait_up = wait_up;
    m->sw_clr_up = clr_up;
    m->sw_write = write_enable;
    membus_write_protect(&m->bus, 0, MCARD_RAM_SIZE, !write_enable);
}

void mcard_data_switch(mcard_t *m, unsigned bit)
{
    m->data_switches ^= (uint8_t)(1u << (bit & 7u));
}

void mcard_in_button(mcard_t *m, bool down)
{
    if (down && !m->in_button)
        m->in_pending = true;
    m->in_button = down;
}

bool mcard_key(mcard_t *m, uint8_t code)
{
    return serial_send(&m->serial, code);
}

/* ------------------------------------------------------------------ */
/*  Running                                                             */
/* ------------------------------------------------------------------ */
/* Author: Thomas Dzubin */
int mcard_cycle(mcard_t *m)
{
    cpu1802_t *c = &m->cpu;
    int kind;

    /* CLR and WAIT are the CLEAR and WAIT lines (both low is LOAD) */
    c->mode = (uint8_t)(((m->sw_clr_up ? 1 : 0) << 1) | (m->sw_wait_up ? 1 : 0));

    /* Rev L: IN held as CLR goes up starts the program at 8000 (R0 is set once
     * the initialization cycle, which clears it, is over) */
    if (m->was_clear && m->sw_clr_up && m->in_button && c->mode == CPU_MODE_RUN)
        m->jump_pending = true;
    m->was_clear = !m->sw_clr_up;
    if (!m->sw_clr_up)
        m->jump_pending = false;

    c->ef = MCARD_EF_FREE;
    if (serial_flag_high(&m->serial))
        c->ef |= MCARD_EF_SERIAL;
    if (!m->in_button)                  /* IN grounds EF4 while it is down */
        c->ef |= MCARD_EF_IN_BUTTON;

    c->int_req = false;
    c->dma_out_req = false;
    if (c->mode != CPU_MODE_LOAD)
        m->in_pending = false;          /* IN only starts a DMA in LOAD mode */
    c->dma_in_req = m->in_pending;

    kind = cpu1802_cycle(c);
    if (kind == CPU_CYCLE_INIT && m->jump_pending) {
        c->r[0] = MCARD_ROM_BASE;
        m->jump_pending = false;
    }
    if (kind != CPU_CYCLE_NONE)
        serial_tick(&m->serial, c->q);
    return kind;
}

uint32_t mcard_run(mcard_t *m, uint32_t cycles)
{
    uint32_t done = 0;

    while (done < cycles) {
        if (mcard_cycle(m) == CPU_CYCLE_NONE)
            break;
        done++;
    }
    return done;
}

static uint32_t machine_run(void *self, uint32_t cycles)
{
    return mcard_run(self, cycles);
}

const machine_t mcard_machine = { "1802 Membership Card", MCARD_CYCLES_PER_SEC, machine_run, NULL };

static void target_remap(void *ctx)
{
    mcard_map_memory(ctx);
}

void mcard_program_target(mcard_t *m, program_target_t *t)
{
    memset(t, 0, sizeof *t);
    t->ram = m->ram;
    t->base = 0;
    t->size = MCARD_RAM_SIZE;
    t->isa = &isa_1802;
    t->bus = &m->bus;
    t->remap = target_remap;
    t->ctx = m;
}
