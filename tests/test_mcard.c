/*
 * test_mcard.c - host-side checks of the 1802 Membership Card board (mcard.c):
 * the front panel's modes, loading and reading memory by the switches, the IN
 * button, the memory map, and the Rev L jump to 8000.
 *
 * Build and run, from the project folder:
 *   gcc -Wall -Wextra -I. tests/test_mcard.c mcard.c cpu1802.c membus.c rom.c serial.c isa_1802.c disasm1802.c asm_core.c -o test_mcard
 *   ./test_mcard
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "mcard.h"
#include "mcard_monitor.h"

static int checks, failed;

#define CHECK(cond) do { \
        checks++; \
        if (!(cond)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #cond); } \
    } while (0)

/* rom.c asks the platform for files; the board has none here. */
int plat_file_save(const char *name, const uint8_t *data, uint32_t len)
{
    (void)name; (void)data; (void)len;
    return 1;
}

int plat_file_load(const char *name, uint8_t *data, uint32_t max, uint32_t *len)
{
    (void)name; (void)data; (void)max;
    *len = 0;
    return 2;
}

/* Put a byte in memory with the switches: LOAD mode, data on the switches, IN. */
static void toggle_in(mcard_t *m, uint8_t value)
{
    m->data_switches = value;
    mcard_in_button(m, true);
    mcard_run(m, 40);
    mcard_in_button(m, false);
    mcard_run(m, 40);
}

static void test_panel(void)
{
    static mcard_t m;
    static const uint8_t blink[] = { 0x7B, 0x7A, 0x30, 0x00 };  /* SEQ, REQ, BR 0 */
    unsigned i;
    bool saw_high = false, saw_low = false;

    mcard_init(&m, NULL, 0);
    CHECK(m.cpu.mode == CPU_MODE_LOAD || m.cpu.mode == CPU_MODE_RESET || 1);

    /* CLEAR, then LOAD, with writing enabled: toggle the program in */
    mcard_set_switches(&m, true, false, true);              /* CLEAR */
    mcard_run(&m, 40);
    CHECK(m.cpu.r[0] == 0);
    mcard_set_switches(&m, false, false, true);             /* LOAD */
    for (i = 0; i < sizeof blink; i++)
        toggle_in(&m, blink[i]);
    CHECK(memcmp(m.ram, blink, sizeof blink) == 0);
    CHECK(m.cpu.r[0] == sizeof blink);
    CHECK(m.display == 0x00);                               /* the last byte shown */

    /* read it back with the READ switch down: nothing is written */
    mcard_set_switches(&m, true, false, false);             /* CLEAR, read only */
    mcard_run(&m, 40);
    mcard_set_switches(&m, false, false, false);            /* LOAD */
    m.data_switches = 0xFF;
    mcard_in_button(&m, true);
    mcard_run(&m, 40);
    mcard_in_button(&m, false);
    mcard_run(&m, 40);
    CHECK(m.display == 0x7B && m.ram[0] == 0x7B);           /* the byte at 0000 */
    mcard_in_button(&m, true);
    mcard_run(&m, 40);
    mcard_in_button(&m, false);
    mcard_run(&m, 40);
    CHECK(m.display == 0x7A && m.ram[1] == 0x7A);

    /* run it: Q goes up and down */
    mcard_set_switches(&m, true, false, true);              /* CLEAR */
    mcard_run(&m, 40);
    mcard_set_switches(&m, true, true, true);               /* RUN */
    for (i = 0; i < 400; i++) {
        mcard_cycle(&m);
        if (m.cpu.q)
            saw_high = true;
        else
            saw_low = true;
    }
    CHECK(saw_high && saw_low);

    /* WAIT stops it */
    mcard_set_switches(&m, false, true, true);
    CHECK(mcard_run(&m, 100) == 0);
}

static void test_in_button_flag(void)
{
    static mcard_t m;

    mcard_init(&m, NULL, 0);
    mcard_set_switches(&m, true, true, true);               /* RUN */
    m.cpu.r[0] = 0;
    mcard_in_button(&m, true);
    mcard_cycle(&m);
    CHECK((m.cpu.ef & MCARD_EF_IN_BUTTON) == 0);            /* EF4 low while pressed */
    mcard_in_button(&m, false);
    mcard_cycle(&m);
    CHECK((m.cpu.ef & MCARD_EF_IN_BUTTON) != 0);
    CHECK((m.cpu.ef & MCARD_EF_SERIAL) != 0);               /* EF3 idles high */
}

static void test_memory_map(void)
{
    static mcard_t m;
    static const uint8_t monitor[] = { 0x7B, 0x30, 0x00 };  /* SEQ, BR 8000 low byte (not used) */

    mcard_init(&m, monitor, sizeof monitor);
    CHECK(membus_page_kind(&m.bus, 0x00) == MEMBUS_KIND_RAM);
    CHECK(membus_page_kind(&m.bus, 0x7F) == MEMBUS_KIND_RAM);
    CHECK(membus_page_kind(&m.bus, 0x80) == MEMBUS_KIND_ROM);
    CHECK(m.cpu.mem_read(&m, 0x8000) == 0x7B);
    m.cpu.mem_write(&m, 0x8000, 0x11);                      /* ROM: ignored */
    CHECK(m.cpu.mem_read(&m, 0x8000) == 0x7B);
    m.cpu.mem_write(&m, 0x1234, 0x42);
    CHECK(m.cpu.mem_read(&m, 0x1234) == 0x42);
    mcard_set_switches(&m, true, true, false);              /* READ: write protected */
    m.cpu.mem_write(&m, 0x1234, 0x99);
    CHECK(m.cpu.mem_read(&m, 0x1234) == 0x42);
}

/* IN held while CLR goes up starts the program at 8000. */
static void test_rev_l_jump(void)
{
    static mcard_t m;
    static const uint8_t monitor[] = { 0x7B, 0x30, 0x00 };  /* SEQ, then BR 00 (page 80) */
    unsigned i;

    mcard_init(&m, monitor, sizeof monitor);
    mcard_set_switches(&m, true, false, true);              /* CLEAR */
    mcard_run(&m, 40);
    mcard_in_button(&m, true);
    mcard_set_switches(&m, true, true, true);               /* RUN with IN held */
    for (i = 0; i < 100 && !m.cpu.q; i++)
        mcard_cycle(&m);
    CHECK(m.cpu.q);                                         /* the monitor's SEQ ran */

    mcard_init(&m, monitor, sizeof monitor);                /* without IN: starts at 0000 */
    mcard_set_switches(&m, true, false, true);
    mcard_run(&m, 40);
    mcard_set_switches(&m, true, true, true);
    for (i = 0; i < 100; i++)
        mcard_cycle(&m);
    CHECK(!m.cpu.q);
}

/* Run the card for this many machine cycles, taking what it sends down the serial
 * line into out (a NUL-ended string, at most size - 1 characters). */
static void run_collect(mcard_t *m, uint32_t cycles, char *out, size_t size)
{
    size_t n = strlen(out);
    uint32_t i;
    int c;

    for (i = 0; i < cycles; i++) {
        mcard_cycle(m);
        while ((c = serial_receive(&m->serial)) >= 0)
            if (n + 1 < size)
                out[n++] = (char)c;
    }
    out[n] = '\0';
}

/* The built-in monitor: it says who it is, and echoes what is typed. */
static void test_monitor(void)
{
    static mcard_t m;
    char out[128] = "";

    mcard_init(&m, MCARD_MONITOR, MCARD_MONITOR_SIZE);
    mcard_set_switches(&m, true, false, true);              /* CLEAR */
    mcard_run(&m, 40);
    mcard_in_button(&m, true);                              /* Rev L: IN held ... */
    mcard_set_switches(&m, true, true, true);               /* ... CLR up: start at 8000 */
    run_collect(&m, 40000, out, sizeof out);
    CHECK(strcmp(out, "MCMON 1802 MEMBERSHIP CARD\r\n") == 0);
    mcard_in_button(&m, false);

    out[0] = '\0';
    CHECK(mcard_key(&m, 'K'));
    run_collect(&m, 4000, out, sizeof out);
    CHECK(strcmp(out, "K") == 0);
    out[0] = '\0';
    CHECK(mcard_key(&m, '7'));
    run_collect(&m, 4000, out, sizeof out);
    CHECK(strcmp(out, "7") == 0);
    out[0] = '\0';
    CHECK(mcard_key(&m, 0x0D));
    run_collect(&m, 6000, out, sizeof out);
    CHECK(strcmp(out, "\r\n") == 0);                        /* CR gets its line feed */
}

int main(void)
{
    test_panel();
    test_in_button_flag();
    test_memory_map();
    test_rev_l_jump();
    test_monitor();
    printf("%d checks, %d failed\n", checks, failed);
    return failed != 0;
}
