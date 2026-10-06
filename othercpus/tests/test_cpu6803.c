/*
 * test_cpu6803.c - checks of the MC6803 core (cpu6803.c): that every opcode the
 * instruction-set description knows takes the cycles and the bytes the data sheet
 * says (and every other one takes the trap vector), programs with known results
 * and cycle counts, the stack, the interrupts (IRQ, NMI, WAI, the delay after CLI),
 * the on-chip timer (overflow, output compare, input capture, the flag clearing
 * sequence), the ports and the on-chip RAM. Plain C, builds with any compiler:
 *
 *     gcc -Wall -Wextra -I. -Iothercpus -DASM_IMAGE_SIZE=0x5000 \
 *         othercpus/tests/test_cpu6803.c othercpus/cpu6803.c asm_core.c \
 *         othercpus/isa_6800.c -o test_cpu6803
 *     ./test_cpu6803
 *
 * The instruction semantics were also checked once against MAME's 6800 opcode
 * code over 896,000 random cases (docs/RESEARCH.md).
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "asm_core.h"
#include "cpu6803.h"
#include "isa.h"
#include "isa_others.h"

static int checks, failures;

#define CHECK(cond) do { \
        checks++; \
        if (!(cond)) { failures++; printf("FAIL line %d: %s\n", __LINE__, #cond); } \
    } while (0)

static uint8_t mem[65536];
static uint8_t last_pins[4], last_ddr[4];
static uint8_t pins_in[4];
static int port_writes, port_reads;
static cpu6803_t cpu;
static asm_t assembler;

static uint8_t bus_read(void *ctx, uint16_t a)
{
    (void)ctx;
    return mem[a];
}

static void bus_write(void *ctx, uint16_t a, uint8_t v)
{
    (void)ctx;
    mem[a] = v;
}

static uint8_t port_in(void *ctx, int port)
{
    (void)ctx;
    port_reads++;
    return pins_in[port - 1];
}

static void port_out(void *ctx, int port, uint8_t pins, uint8_t ddr)
{
    (void)ctx;
    last_pins[port - 1] = pins;
    last_ddr[port - 1] = ddr;
    port_writes++;
}

/* Power on with the reset vector pointing at start. */
static void power_on(uint16_t start)
{
    memset(mem, 0, sizeof mem);
    mem[0xFFFE] = (uint8_t)(start >> 8);
    mem[0xFFFF] = (uint8_t)start;
    memset(pins_in, 0xFF, sizeof pins_in);
    memset(&cpu, 0, sizeof cpu);
    cpu.mem_read = bus_read;
    cpu.mem_write = bus_write;
    cpu.port_in = port_in;
    cpu.port_out = port_out;
    cpu6803_reset(&cpu);
    cpu.sp = 0x4FFF;
}

/* Assemble source (at its own ORG) into mem. Returns the number of errors. */
static int load(const char *source)
{
    int errors = asm_assemble(&isa_6803, &assembler, 0, ASM_IMAGE_SIZE, source, strlen(source));

    if (errors == 0)
        memcpy(mem + assembler.low, assembler.image + assembler.low,
               (size_t)assembler.bytes);
    return errors;
}

/* Run n instructions; returns the machine cycles they took. */
static int run(int n)
{
    int total = 0;

    while (n-- > 0)
        total += cpu6803_step(&cpu);
    return total;
}

/* Run until the PC reaches addr (at most limit steps); returns the cycles. */
static int run_to(uint16_t addr, int limit)
{
    int total = 0;

    while (cpu.pc != addr && limit-- > 0)
        total += cpu6803_step(&cpu);
    return total;
}

/* ---------------------------------------------------------------------- */

/* Author: Thomas Dzubin */
static void test_opcode_table(void)
{
    static const uint8_t cycles[256] = CPU6803_CYCLES;
    int op;

    for (op = 0; op < 256; op++) {
        uint8_t b[ISA_MAX_BYTES];
        isa_flow_t f;
        int n;

        memset(b, 0x90, sizeof b);          /* operands that land in RAM, not the registers */
        b[0] = (uint8_t)op;
        isa_6803.flow(0x0200, b, &f);
        power_on(0x0200);
        cpu.x = 0x4000;
        memcpy(mem + 0x0200, b, sizeof b);
        n = cpu6803_step(&cpu);
        if (f.kind == ISA_FLOW_BAD) {
            CHECK(cpu.trapped);
            CHECK(n == M68_INT_CYCLES);
        } else {
            CHECK(!cpu.trapped);
            CHECK(n == cycles[op]);
            if (f.kind == ISA_FLOW_NEXT)
                CHECK(cpu.pc == 0x0200 + f.length);
            if (op != 0x3E && op != 0x3F)      /* WAI and SWI go elsewhere */
                CHECK(f.kind != ISA_FLOW_BAD);
        }
    }
}

/* Author: Thomas Dzubin */
static void test_arithmetic(void)
{
    /* the data sheet's cycle counts for a small loop: 2 + 5 * (2 + 3) */
    power_on(0x0100);
    CHECK(load("ORG $0100\n LDAA #5\nL1: DECA\n BNE L1\n WAI\n") == 0);
    CHECK(run(1 + 10) == 2 + 5 * 5);
    CHECK(cpu.a == 0 && (cpu.cc & M68_CC_Z));

    power_on(0x0100);
    CHECK(load("ORG $0100\n LDAA #$7F\n ADDA #1\n") == 0);
    run(2);
    CHECK(cpu.a == 0x80 && (cpu.cc & M68_CC_V) && (cpu.cc & M68_CC_N) && !(cpu.cc & M68_CC_C));

    power_on(0x0100);
    CHECK(load("ORG $0100\n LDAA #$FF\n ADDA #1\n") == 0);
    run(2);
    CHECK(cpu.a == 0 && (cpu.cc & M68_CC_C) && (cpu.cc & M68_CC_Z) && (cpu.cc & M68_CC_H));

    power_on(0x0100);
    CHECK(load("ORG $0100\n LDD #$1234\n ADDD #$0FFF\n SUBD #$0001\n") == 0);
    run(3);
    CHECK(cpu.a == 0x22 && cpu.b == 0x32);

    power_on(0x0100);
    CHECK(load("ORG $0100\n LDAA #$12\n LDAB #$34\n MUL\n") == 0);
    run(3);
    CHECK(((cpu.a << 8) | cpu.b) == 0x12 * 0x34);

    power_on(0x0100);
    CHECK(load("ORG $0100\n LDAA #$19\n ADDA #$28\n DAA\n") == 0);
    run(3);
    CHECK(cpu.a == 0x47);

    power_on(0x0100);
    CHECK(load("ORG $0100\n LDD #$8001\n ASLD\n LSRD\n") == 0);
    run(2);
    CHECK(cpu.a == 0x00 && cpu.b == 0x02 && (cpu.cc & M68_CC_C));
    run(1);
    CHECK(cpu.a == 0x00 && cpu.b == 0x01 && !(cpu.cc & M68_CC_C));

    power_on(0x0100);
    CHECK(load("ORG $0100\n LDX #$1000\n LDAB #$FF\n ABX\n") == 0);
    run(3);
    CHECK(cpu.x == 0x10FF);

    power_on(0x0100);
    CHECK(load("ORG $0100\n LDAA #$80\n NEGA\n") == 0);
    run(2);
    CHECK(cpu.a == 0x80 && (cpu.cc & M68_CC_V) && (cpu.cc & M68_CC_C));

    power_on(0x0100);
    CHECK(load("ORG $0100\n LDAA #$81\n ASRA\n RORA\n") == 0);
    run(2);
    CHECK(cpu.a == 0xC0 && (cpu.cc & M68_CC_C));
    run(1);
    CHECK(cpu.a == 0xE0 && !(cpu.cc & M68_CC_C));
}

/* Author: Thomas Dzubin */
static void test_stack_and_calls(void)
{
    power_on(0x0100);
    CHECK(load("ORG $0100\n LDS #$4F00\n LDAA #$11\n PSHA\n LDAA #$22\n PSHA\n PULB\n PULA\n"
               " LDX #$ABCD\n PSHX\n PULX\n TSX\n") == 0);
    run(7);
    CHECK(cpu.a == 0x11 && cpu.b == 0x22 && cpu.sp == 0x4F00);
    run(3);                                      /* LDX, PSHX, PULX */
    CHECK(cpu.x == 0xABCD && cpu.sp == 0x4F00);
    run(1);                                      /* TSX */
    CHECK(cpu.x == 0x4F01);

    power_on(0x0100);
    CHECK(load("ORG $0100\n LDS #$4F00\n JSR sub\n BRA done\nsub: LDAA #$5A\n RTS\ndone: NOP\n") == 0);
    CHECK(run(2) == 3 + 6);                      /* LDS immediate 3, JSR extended 6 */
    CHECK(mem[0x4F00] == 0x06 && mem[0x4EFF] == 0x01);   /* the return address, $0106 */
    run(1);                                      /* LDAA */
    CHECK(cpu.a == 0x5A);
    CHECK(run(1) == 5);                          /* RTS */
    CHECK(cpu.pc == 0x0106);

    power_on(0x0100);
    CHECK(load("ORG $0100\n LDS #$4F00\n BSR sub\n NOP\nsub: RTS\n") == 0);
    CHECK(run(2) == 3 + 6);
    CHECK(run(1) == 5);
    CHECK(cpu.pc == 0x0105);

    power_on(0x0100);
    CHECK(load("ORG $0100\n LDS #$4F00\n SWI\n NOP\n") == 0);
    mem[0xFFFA] = 0x02;
    mem[0xFFFB] = 0x00;
    CHECK(run(1) == 3);
    CHECK(run(1) == 12);
    CHECK(cpu.pc == 0x0200 && (cpu.cc & M68_CC_I));
    mem[0x0200] = 0x3B;                         /* RTI */
    CHECK(run(1) == 10);
    CHECK(cpu.pc == 0x0104);
}

/* Author: Thomas Dzubin */
static void test_interrupts(void)
{
    /* IRQ1: taken between instructions when I is clear, 12 cycles */
    power_on(0x0100);
    CHECK(load("ORG $0100\n LDS #$4F00\n CLI\n NOP\n NOP\n NOP\n") == 0);
    mem[0xFFF8] = 0x03;
    mem[0xFFF9] = 0x00;
    run(2);                                      /* LDS, CLI */
    cpu.irq = true;
    CHECK(run(1) == 2);                          /* the NOP after CLI runs first */
    CHECK(cpu.pc == 0x0105);
    CHECK(run(1) == M68_INT_CYCLES);
    CHECK(cpu.pc == 0x0300 && (cpu.cc & M68_CC_I));
    CHECK(mem[0x4F00] == 0x05 && mem[0x4EFF] == 0x01);   /* the return address, low byte first */
    cpu.irq = false;

    /* masked while I is set */
    power_on(0x0100);
    CHECK(load("ORG $0100\n NOP\n NOP\n") == 0);
    cpu.irq = true;
    CHECK(run(1) == 2 && cpu.pc == 0x0101);

    /* NMI is taken whatever I is, once per falling edge */
    power_on(0x0100);
    CHECK(load("ORG $0100\n NOP\n NOP\n") == 0);
    mem[0xFFFC] = 0x04;
    mem[0xFFFD] = 0x00;
    mem[0x0400] = 0x01;                          /* NOPs at the handler */
    mem[0x0401] = 0x01;
    cpu6803_nmi(&cpu, true);
    CHECK(run(1) == M68_INT_CYCLES && cpu.pc == 0x0400);
    CHECK(run(1) == 2 && cpu.pc == 0x0401);      /* then the handler goes on, no second NMI */

    /* WAI stacks, waits, and an interrupt then takes only the vector fetch */
    power_on(0x0100);
    CHECK(load("ORG $0100\n LDS #$4F00\n CLI\n NOP\n WAI\n NOP\n") == 0);
    mem[0xFFF8] = 0x03;
    mem[0xFFF9] = 0x00;
    run(4);                                      /* LDS CLI NOP WAI */
    CHECK(cpu.wai);
    CHECK(run(1) == 1 && cpu.wai);               /* idle */
    cpu.irq = true;
    CHECK(run(1) == M68_INT_WAI_CYCLES);
    CHECK(!cpu.wai && cpu.pc == 0x0300);
}

/* Author: Thomas Dzubin */
static void test_timer(void)
{
    /* the counter runs one count per cycle from reset */
    power_on(0x0100);
    CHECK(load("ORG $0100\n NOP\n NOP\n NOP\n") == 0);
    run(3);
    CHECK(cpu.counter == 6);

    /* overflow: TOF is set when the counter reaches $FFFF, and reading TCSR then the
     * counter high byte clears it */
    power_on(0x0100);
    CHECK(load("ORG $0100\n LDD #$FFF0\n STD $09\n NOP\n NOP\n NOP\n NOP\n NOP\n NOP\n NOP\n"
               " NOP\n LDAA $08\n LDAB $09\n LDAA $08\n") == 0);
    run(2);                                      /* LDD, STD: the counter is set at the STD */
    CHECK(cpu.counter >= 0xFFF0);
    run(8);
    CHECK(cpu.tcsr & M68_TCSR_TOF);
    run(1);                                      /* LDAA $08 */
    CHECK(cpu.a & M68_TCSR_TOF);
    run(1);                                      /* LDAB $09 clears it */
    CHECK(!(cpu.tcsr & M68_TCSR_TOF));

    /* output compare sets OCF, an interrupt with EOCI and I clear, and drives P21 */
    power_on(0x0100);
    CHECK(load("ORG $0100\n LDS #$4F00\n LDD #100\n STD $0B\n LDAA #$02\n STAA $01\n"
               " LDAA #$09\n STAA $08\n CLI\nloop: BRA loop\n") == 0);
    mem[0xFFF4] = 0x05;
    mem[0xFFF5] = 0x00;
    mem[0x0500] = 0x01;                          /* NOP at the handler */
    port_writes = 0;
    run_to(0x0500, 100);
    CHECK(cpu.pc == 0x0500);
    CHECK(cpu.tcsr & M68_TCSR_OCF);
    CHECK(cpu.counter >= 100 && cpu.counter < 100 + 20);
    CHECK((last_pins[1] & M68_P2_OC_BIT) != 0);  /* OLVL was set in TCSR: P21 high */
    /* writing OCR after reading TCSR clears OCF */
    cpu.pc = 0x0600;
    mem[0x0600] = 0x96; mem[0x0601] = 0x08;      /* LDAA $08 */
    mem[0x0602] = 0x97; mem[0x0603] = 0x0C;      /* STAA $0C */
    cpu.cc |= M68_CC_I;
    run(2);
    CHECK(!(cpu.tcsr & M68_TCSR_OCF));

    /* input capture: the selected edge latches the counter */
    power_on(0x0100);
    CHECK(load("ORG $0100\n NOP\n NOP\n") == 0);
    cpu.tcsr = M68_TCSR_IEDG;                    /* rising edge */
    run(2);
    cpu6803_input_capture(&cpu, false);
    CHECK(!(cpu.tcsr & M68_TCSR_ICF));
    cpu6803_input_capture(&cpu, true);
    CHECK((cpu.tcsr & M68_TCSR_ICF) && cpu.icr == 4);

    /* the counter is held when its high byte is read, so the low byte is consistent */
    power_on(0x0100);
    CHECK(load("ORG $0100\n LDD $09\n") == 0);
    cpu.counter = 0x12FD;
    run(1);
    CHECK(cpu.a == 0x12 && cpu.b == 0xFD);        /* the low byte is the one held with the high */
}

/* Author: Thomas Dzubin */
static void test_ports_and_ram(void)
{
    power_on(0x0100);
    CHECK(load("ORG $0100\n LDAA #$FF\n STAA $00\n LDAA #$A5\n STAA $02\n LDAA #$1F\n STAA $01\n"
               " LDAA $02\n") == 0);
    port_writes = 0;
    run(2);                                      /* DDR1 all outputs */
    CHECK(last_ddr[0] == 0xFF);
    run(2);
    CHECK(last_pins[0] == 0xA5);
    run(2);
    CHECK(last_ddr[1] == 0x1F);
    pins_in[0] = 0x00;
    run(1);
    CHECK(cpu.a == 0xA5);                        /* all output: reads what was written */

    /* inputs read the pins */
    power_on(0x0100);
    CHECK(load("ORG $0100\n LDAA $02\n LDAB $03\n") == 0);
    pins_in[0] = 0x3C;
    pins_in[1] = 0x1B;
    run(2);
    CHECK(cpu.a == 0x3C && cpu.b == 0x1B);

    /* the on-chip RAM answers at $80 to $FF and the bus never sees it */
    power_on(0x0100);
    CHECK(load("ORG $0100\n LDAA #$5A\n STAA $90\n LDAB $90\n LDAA #$77\n STAA $2000\n") == 0);
    run(5);
    CHECK(cpu.b == 0x5A && mem[0x90] == 0 && mem[0x2000] == 0x77);
    CHECK(cpu.ram[0x90 - M68_RAM_BASE] == 0x5A);
}

/* Author: Thomas Dzubin */
static void test_clr_reads_first(void)
{
    /* CLR reads its operand and then writes the zero. At a port register the read
     * is seen by the machine (it asks for the pins), and the zero is written. */
    power_on(0x0100);
    CHECK(load("ORG $0100\n CLR $0002\n") == 0);
    CHECK(mem[0x0100] == 0x7F);                  /* the extended form */
    cpu.data[0] = 0x55;
    port_reads = 0;
    run(1);
    CHECK(port_reads == 1);
    CHECK(cpu.data[0] == 0);
    CHECK((cpu.cc & M68_CC_Z) && !(cpu.cc & (M68_CC_N | M68_CC_V | M68_CC_C)));
}

/* The counter moves a whole instruction at a time. For counters near the wrap
 * and compare values near the counter, instructions of 2, 4 and 10 cycles must
 * set the flags that counting one clock at a time sets.
 *
 * Author: Thomas Dzubin */
static void test_timer_stretches(void)
{
    static const uint8_t ops[3][3] = { { 0x01, 0, 0 }, { 0xB6, 0x40, 0x00 }, { 0x3D, 0, 0 } };
    static const uint16_t starts[] = { 0xFFF0, 0xFFF5, 0xFFFC, 0xFFFD, 0xFFFE, 0xFFFF, 0x0000,
                                       0x0001, 0x0004, 0x7FF0, 0x7FFB };
    int o, s, d, bad = 0;

    for (o = 0; o < 3; o++) {
        for (s = 0; s < (int)(sizeof starts / sizeof starts[0]); s++) {
            for (d = -3; d <= 14; d++) {
                uint16_t c0 = starts[s], ocr = (uint16_t)(c0 + d), k = c0;
                bool ocf = false, tof = false;
                int n, i;

                power_on(0x0100);
                memcpy(mem + 0x0100, ops[o], 3);
                cpu.counter = c0;
                cpu.ocr = ocr;
                n = cpu6803_step(&cpu);
                for (i = 0; i < n; i++) {
                    k++;
                    if (k == ocr)
                        ocf = true;
                    if (k == 0xFFFF)
                        tof = true;
                }
                if (cpu.counter != k || ((cpu.tcsr & M68_TCSR_OCF) != 0) != ocf ||
                    ((cpu.tcsr & M68_TCSR_TOF) != 0) != tof)
                    bad++;
            }
        }
    }
    CHECK(bad == 0);
}

/* The assembler puts code in the window of the address space it is given, at the
 * real addresses.
 *
 * Author: Thomas Dzubin */
static void test_assembler_window(void)
{
    const char *rom = "ORG $E000\n LDAA #$12\nstart: BRA start\nORG $FFFE\n DW start\n";
    const char *below = "ORG $DFFF\n NOP\n";
    const char *top = "ORG $5000\n NOP\n";
    const char *edge = "ORG $4FFF\n NOP\n";

    CHECK(asm_assemble(&isa_6803, &assembler, 0xE000, 0x2000, rom, strlen(rom)) == 0);
    CHECK(assembler.low == 0xE000 && assembler.high == 0xFFFF && assembler.bytes == 6);
    CHECK(assembler.image[0] == 0x86 && assembler.image[1] == 0x12);
    CHECK(assembler.image[0x1FFE] == 0xE0 && assembler.image[0x1FFF] == 0x02);
    CHECK(assembler.lines[1].address == 0xE000);

    CHECK(asm_assemble(&isa_6803, &assembler, 0xE000, 0x2000, below, strlen(below)) == 1);
    CHECK(strcmp(assembler.errors[0].text, ASM_ERR_RAM) == 0);
    CHECK(asm_assemble(&isa_6803, &assembler, 0x4000, 0x1000, edge, strlen(edge)) == 0);
    CHECK(assembler.low == 0x4FFF && assembler.image[0x0FFF] == 0x01);
    CHECK(asm_assemble(&isa_6803, &assembler, 0x4000, 0x1000, top, strlen(top)) == 1);
    /* a window bigger than the assembler's image is cut to it */
    CHECK(asm_assemble(&isa_6803, &assembler, 0, 0x10000, top, strlen(top)) == 1);
    CHECK(asm_assemble(&isa_6803, &assembler, 0, 0x10000, edge, strlen(edge)) == 0);
}

int main(void)
{
    test_opcode_table();
    test_arithmetic();
    test_stack_and_calls();
    test_interrupts();
    test_timer();
    test_ports_and_ram();
    test_clr_reads_first();
    test_timer_stretches();
    test_assembler_window();
    printf("%d checks, %d failed\n", checks, failures);
    return failures != 0;
}
