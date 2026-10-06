/*
 * elf.c - the Elf board: wires the CPU, the video chip, the RAM and the front
 * panel together. See elf.h.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "elf.h"
#include "isa.h"
#include "rom.h"

/* ------------------------------------------------------------------ */
/*  The bus the CPU sees                                                */
/* ------------------------------------------------------------------ */
static uint8_t bus_mem_read(void *ctx, uint16_t addr)
{
    const elf_t *m = ctx;

    return membus_read(&m->bus, addr);
}

static void bus_mem_write(void *ctx, uint16_t addr, uint8_t data)
{
    const elf_t *m = ctx;

    membus_write(&m->bus, addr, data);
}

/* The VDU's video RAM is read like memory; a write is stored and counted. */
static void vdu_write(void *ctx, uint16_t addr, uint8_t data)
{
    elf_t *m = ctx;

    m->vdu[addr - ELF_VDU_BASE] = data;
    m->vdu_writes++;
}

/* Author: Thomas Dzubin */
static uint8_t bus_io_in(void *ctx, uint8_t port)
{
    elf_t *m = ctx;

    switch (port) {
    case ELF_PORT_VIDEO:
        if (m->video_installed)
            pixie_display_on(&m->video);
        return ELF_FLOATING_BUS;
    case ELF_PORT_KEYPAD:
        return m->key_latch;
    case ELF_PORT_ASCII:
        m->kbd_polls++;
        if (m->kbd_ready) {                 /* the key has been taken */
            m->kbd_ready = false;
            m->kbd_gap = ELF_KBD_GAP_CYCLES;
        }
        return m->kbd_latch;
    default:
        return ELF_FLOATING_BUS;
    }
}

static void bus_io_out(void *ctx, uint8_t port, uint8_t data)
{
    elf_t *m = ctx;

    switch (port) {
    case ELF_PORT_VIDEO:
        if (m->video_installed)
            pixie_display_off(&m->video);
        break;
    case ELF_PORT_DISPLAY:
        m->display = data;
        break;
    default:
        break;
    }
}

/* DMA-IN is the IN button in load mode: the keypad's two digits go to RAM. */
static uint8_t bus_dma_in(void *ctx)
{
    elf_t *m = ctx;

    m->in_pending = false;
    m->display = m->key_latch;
    return m->key_latch;
}

/* DMA-OUT is the video chip taking a picture byte. */
static void bus_dma_out(void *ctx, uint8_t data)
{
    elf_t *m = ctx;

    pixie_dma_byte(&m->video, data);
}

/* The memory map: the RAM, the VDU's video RAM, and any ROM images over them. It is
 * made again when a ROM is fitted or taken out (the memory protect switch stays as
 * it was). */
static void map_memory(elf_t *m)
{
    membus_init(&m->bus, ELF_UNMAPPED_READ);
    membus_map_ram(&m->bus, 0, ELF_RAM_SIZE, m->ram);
    membus_map_hooked(&m->bus, ELF_VDU_BASE, ELF_VDU_SIZE, m->vdu, vdu_write, m);
    rom_apply(&m->bus);
    membus_write_protect(&m->bus, 0, ELF_RAM_SIZE, m->sw_mp);
}

/* ------------------------------------------------------------------ */
/*  Set-up and front panel                                              */
/* ------------------------------------------------------------------ */
void elf_init(elf_t *m)
{
    memset(m, 0, sizeof *m);
    m->video_installed = true;
    map_memory(m);
    pixie_init(&m->video);
    qaudio_init(&m->audio, ELF_CYCLES_PER_SEC);
    cpu1802_power_on(&m->cpu);
    m->cpu.ctx = m;
    m->cpu.mem_read = bus_mem_read;
    m->cpu.mem_write = bus_mem_write;
    m->cpu.io_in = bus_io_in;
    m->cpu.io_out = bus_io_out;
    m->cpu.dma_in = bus_dma_in;
    m->cpu.dma_out = bus_dma_out;
}

void elf_set_video(elf_t *m, bool installed)
{
    m->video_installed = installed;
    if (!installed)
        pixie_display_off(&m->video);
}

void elf_set_switches(elf_t *m, bool run, bool load, bool mp)
{
    m->sw_run = run;
    m->sw_load = load;
    m->sw_mp = mp;
    membus_write_protect(&m->bus, 0, ELF_RAM_SIZE, mp);     /* the VDU is left alone */
}

void elf_key_hex(elf_t *m, unsigned digit)
{
    m->key_latch = (uint8_t)((m->key_latch << 4) | (digit & 0x0F));
    if (m->cpu.mode == CPU_MODE_LOAD)
        m->display = m->key_latch;      /* the displays follow the keypad */
}

void elf_in_button(elf_t *m, bool down)
{
    if (down && !m->in_button)
        m->in_pending = true;
    m->in_button = down;
}

/* The next waiting key goes into the latch, if the latch is free and the gap
 * since the last key has passed. */
static void kbd_present(elf_t *m)
{
    if (m->kbd_ready || m->kbd_gap > 0 || m->kbd_count == 0)
        return;
    m->kbd_latch = m->kbd[m->kbd_head];
    m->kbd_head = (uint8_t)((m->kbd_head + 1) % ELF_KBD_FIFO);
    m->kbd_count--;
    m->kbd_ready = true;
}

bool elf_ascii_key(elf_t *m, uint8_t code)
{
    if (m->kbd_count >= ELF_KBD_FIFO)
        return false;
    m->kbd[(m->kbd_head + m->kbd_count) % ELF_KBD_FIFO] = code;
    m->kbd_count++;
    kbd_present(m);
    return true;
}

void elf_load_image(elf_t *m, const uint8_t *data, uint32_t len)
{
    if (len > ELF_RAM_SIZE)
        len = ELF_RAM_SIZE;
    if (len > 0)
        memcpy(m->ram, data, len);
    memset(m->ram + len, 0, ELF_RAM_SIZE - len);
    elf_clear_vdu(m);                       /* a new program, a blank screen */
}

void elf_clear_vdu(elf_t *m)
{
    memset(m->vdu, 0, sizeof m->vdu);
}

/* ------------------------------------------------------------------ */
/*  Running                                                             */
/* ------------------------------------------------------------------ */
/* Author: Thomas Dzubin */
int elf_cycle(elf_t *m)
{
    cpu1802_t *c = &m->cpu;
    bool q_before = c->q;
    int kind;

    /* CLEAR is the RUN switch, WAIT is the LOAD switch turned over. */
    c->mode = (uint8_t)(((m->sw_run ? 1 : 0) << 1) | (m->sw_load ? 0 : 1));
    if (c->mode == CPU_MODE_RESET)      /* the 1861 is on the same CLEAR line: */
        pixie_display_off(&m->video);   /* RESET turns its picture off         */

    c->ef = ELF_EF_UNUSED;
    if (!m->video_installed || !pixie_efx_low(&m->video))
        c->ef |= ELF_EF_VIDEO;
    if (m->kbd_gap > 0)
        m->kbd_gap--;
    kbd_present(m);
    if (!m->kbd_ready)                      /* the strobe pulls EF3 low */
        c->ef |= ELF_EF_KEYBOARD;
    if (!m->in_button)                      /* IN grounds EF4 while it is down */
        c->ef |= ELF_EF_IN_BUTTON;

    c->int_req = m->video_installed && pixie_int_request(&m->video);
    c->dma_out_req = m->video_installed && pixie_dma_request(&m->video);
    if (c->mode != CPU_MODE_LOAD)
        m->in_pending = false;          /* IN only starts a DMA in load mode */
    c->dma_in_req = m->in_pending;

    kind = cpu1802_cycle(c);
    if (kind == CPU_CYCLE_S0 && c->i == 0x3 &&
        (c->n == 0x6 || c->n == 0xE))       /* B3 or BN3: EF3 is being tested */
        m->kbd_polls++;
    if (kind != CPU_CYCLE_NONE) {
        if (m->video_installed)
            pixie_cycle_done(&m->video);
        qaudio_cycle(&m->audio, c->q);
    }
    if (c->q != q_before) {
        m->q_changes++;
        if (c->q) {                         /* a rising edge: time the cycle */
            if (m->q_rise_known)
                m->q_period = c->cycles - m->q_rise_cycle;
            m->q_rise_cycle = c->cycles;
            m->q_rise_known = true;
        }
    }
    return kind;
}

uint32_t elf_run(elf_t *m, uint32_t budget)
{
    uint32_t done = 0;

    while (done < budget) {
        if (elf_cycle(m) == CPU_CYCLE_NONE)
            break;
        done++;
    }
    return done;
}

uint32_t elf_step(elf_t *m)
{
    uint32_t done = 0;

    while (done < ELF_STEP_LIMIT) {
        uint8_t before = m->cpu.phase;
        int kind = elf_cycle(m);

        if (kind == CPU_CYCLE_NONE)
            break;
        done++;
        /* the instruction is over when its last S1 leaves the CPU at a
         * decision point (or in IDL) */
        if (kind == CPU_CYCLE_S1 &&
            (before == CPU_PHASE_EXEC || before == CPU_PHASE_EXEC2) &&
            (m->cpu.phase == CPU_PHASE_NEXT || m->cpu.phase == CPU_PHASE_IDLE))
            break;
    }
    return done;
}

/* Author: Thomas Dzubin */
uint32_t elf_run_to(elf_t *m, uint32_t budget, uint16_t break_addr, bool *hit)
{
    uint32_t done = 0;
    bool executed = false;

    *hit = false;
    while (done < budget) {
        int kind = elf_cycle(m);

        if (kind == CPU_CYCLE_NONE)
            break;
        done++;
        if (kind == CPU_CYCLE_S1)
            executed = true;
        if (executed && m->cpu.phase == CPU_PHASE_NEXT &&
            m->cpu.r[m->cpu.p] == break_addr) {
            *hit = true;
            break;
        }
    }
    return done;
}

static void target_loaded(void *ctx)
{
    elf_clear_vdu(ctx);                     /* a new program, a blank screen */
}

static const char *target_video_text(void *ctx)
{
    const elf_t *m = ctx;

    return m->video_installed ? "1861 video chip: FITTED" : "1861 video chip: NONE";
}

static bool target_video_press(void *ctx, char *note, size_t note_size)
{
    elf_t *m = ctx;

    elf_set_video(m, !m->video_installed);      /* a basic Elf, or an Elf II */
    snprintf(note, note_size, "%s", m->video_installed ? "1861 FITTED (ELF II)" : "NO 1861: A BASIC ELF");
    return true;
}

static void target_remap(void *ctx)
{
    map_memory(ctx);
}

void elf_program_target(elf_t *m, program_target_t *t)
{
    t->ram = m->ram;
    t->base = 0;
    t->size = ELF_RAM_SIZE;
    t->isa = &isa_1802;
    t->loaded = target_loaded;
    t->extra_key = 'y';
    t->extra_text = target_video_text;
    t->extra_press = target_video_press;
    t->bus = &m->bus;
    t->remap = target_remap;
    t->ctx = m;
}

static uint32_t machine_run(void *self, uint32_t cycles)
{
    return elf_run(self, cycles);
}

static uint32_t machine_run_to(void *self, uint32_t cycles, uint16_t address, bool *hit)
{
    return elf_run_to(self, cycles, address, hit);
}

const machine_t elf_machine = { "Elf II", ELF_CYCLES_PER_SEC, machine_run, machine_run_to };

uint8_t elf_peek(const elf_t *m, uint16_t addr)
{
    return membus_peek(&m->bus, addr);
}
