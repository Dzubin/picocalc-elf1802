/*
 * test_cpuz80.c - checks of the Z80 core (cpuz80.c): that every unprefixed
 * opcode it runs takes the T-states and the bytes the data sheet says (and every
 * one it does not run yet is flagged), every 8-bit arithmetic and logic result and
 * flag against an independent calculation, INC and DEC, decimal adjust against
 * BCD arithmetic, the 16-bit and accumulator flag instructions, programs
 * assembled with the project's own Z80 assembler, the stack and calls, the I/O
 * ports, the interrupts (NMI, modes 0, 1 and 2, EI's delay, HALT) and the R
 * register. Plain C, builds with any compiler:
 *
 *     gcc -Wall -Wextra -I. -Iothercpus -DASM_IMAGE_SIZE=0x5000 \
 *         othercpus/tests/test_cpuz80.c othercpus/cpuz80.c asm_core.c \
 *         othercpus/isa_z80.c -o test_cpuz80
 *     ./test_cpuz80
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "asm_core.h"
#include "cpuz80.h"
#include "isa.h"
#include "isa_others.h"

static int checks, failures;

#define CHECK(cond) do { \
        checks++; \
        if (!(cond)) { failures++; printf("FAIL line %d: %s\n", __LINE__, #cond); } \
    } while (0)

static uint8_t mem[65536];
static cpuz80_t cpu;
static asm_t assembler;
static uint16_t last_port;
static uint8_t last_out, in_value;
static int io_reads, io_writes;

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

static uint8_t port_in(void *ctx, uint16_t port)
{
    (void)ctx;
    last_port = port;
    io_reads++;
    return in_value;
}

static void port_out(void *ctx, uint16_t port, uint8_t v)
{
    (void)ctx;
    last_port = port;
    last_out = v;
    io_writes++;
}

/* Power on with memory clear, SP at 8000H and the program at 0. */
static void power_on(void)
{
    memset(mem, 0, sizeof mem);
    memset(&cpu, 0, sizeof cpu);
    cpu.mem_read = bus_read;
    cpu.mem_write = bus_write;
    cpu.io_in = port_in;
    cpu.io_out = port_out;
    cpuz80_reset(&cpu);
    cpu.sp = 0x8000;
    io_reads = io_writes = 0;
}

/* Assemble source (at its own ORG) into mem. Returns the number of errors. */
static int load(const char *source)
{
    int errors = asm_assemble(&isa_z80, &assembler, 0, ASM_IMAGE_SIZE, source, strlen(source));

    if (errors == 0)                                    /* from the lowest to the highest byte */
        memcpy(mem + assembler.low, assembler.image + assembler.low,
               (size_t)(assembler.high - assembler.low + 1));
    return errors;
}

static int run(int n)
{
    int total = 0;

    while (n-- > 0)
        total += cpuz80_step(&cpu);
    return total;
}

/* Run until HALT (at most limit steps); returns the T-states. */
static int run_to_halt(int limit)
{
    int total = 0;

    while (!cpu.halted && limit-- > 0)
        total += cpuz80_step(&cpu);
    return total;
}

static uint16_t hl(void)  { return (uint16_t)((cpu.h << 8) | cpu.l); }
static uint16_t de(void)  { return (uint16_t)((cpu.d << 8) | cpu.e); }

/* ---------------------------------------------------------------------- */

/* Is the condition in bits 5-3 of the opcode true for the flags f? */
static bool cc_true(int y, uint8_t f)
{
    switch (y) {
    case 0:  return !(f & Z80_F_Z);
    case 1:  return (f & Z80_F_Z) != 0;
    case 2:  return !(f & Z80_F_C);
    case 3:  return (f & Z80_F_C) != 0;
    case 4:  return !(f & Z80_F_PV);
    case 5:  return (f & Z80_F_PV) != 0;
    case 6:  return !(f & Z80_F_S);
    default: return (f & Z80_F_S) != 0;
    }
}

/* Every unprefixed opcode: the T-states (conditional ones with the condition true
 * and false), where PC ends up, and that the ones not run yet are flagged.
 *
 * Author: Thomas Dzubin */
static void test_opcode_table(void)
{
    static const uint8_t table[256] = CPUZ80_TSTATES;
    int op, fi;
    int run_count = 0;

    for (op = 0; op < 256; op++) {
        for (fi = 0; fi < 2; fi++) {
            uint8_t f = fi ? 0xFF : 0x00;
            uint8_t b[ISA_MAX_BYTES] = { (uint8_t)op, 0x00, 0x60, 0, 0 };
            isa_flow_t fl;
            int y = (op >> 3) & 7;
            int expect_t, t;
            bool taken = false;
            uint16_t expect_pc;

            power_on();
            cpu.f = f;
            cpu.h = 0x40;                               /* HL = 4000H, BC = 4100H, DE = 4200H */
            cpu.b = 0x41;
            cpu.d = 0x42;
            mem[0] = (uint8_t)op;
            mem[1] = 0x00;
            mem[2] = 0x60;                              /* the operand: 6000H, or n = 0 */
            mem[cpu.sp] = 0x34;                         /* what RET finds on the stack */
            mem[cpu.sp + 1] = 0x12;
            t = cpuz80_step(&cpu);

            if (table[op] == 0) {                       /* not run yet: a one-byte NOP, flagged */
                CHECK(cpu.unsupported && cpu.unsupported_op == op);
                CHECK(t == Z80_T_UNSUPPORTED && cpu.pc == 1);
                continue;
            }
            run_count++;
            CHECK(!cpu.unsupported);
            isa_z80.flow(0, b, &fl);
            expect_t = table[op];
            expect_pc = (uint16_t)fl.length;
            if ((op & 0xC7) == 0xC0) {                  /* RET cc */
                taken = cc_true(y, f);
                expect_pc = taken ? 0x1234 : 1;
                expect_t += taken ? Z80_T_RET_TAKEN : 0;
            } else if ((op & 0xC7) == 0xC4) {           /* CALL cc,nn */
                taken = cc_true(y, f);
                expect_pc = taken ? 0x6000 : 3;
                expect_t += taken ? Z80_T_CALL_TAKEN : 0;
            } else if ((op & 0xC7) == 0xC2) {           /* JP cc,nn */
                expect_pc = cc_true(y, f) ? 0x6000 : 3;
            } else if (op == 0xC3) {
                expect_pc = 0x6000;
            } else if (op == 0xCD) {
                expect_pc = 0x6000;
            } else if (op == 0xC9) {
                expect_pc = 0x1234;
            } else if (op == 0xE9) {
                expect_pc = 0x4000;
            } else if ((op & 0xC7) == 0xC7) {           /* RST */
                expect_pc = (uint16_t)(op & 0x38);
            }
            CHECK(t == expect_t);
            CHECK(cpu.pc == expect_pc);
        }
    }
    CHECK(run_count == 2 * (256 - 12));                 /* all but the 12 not run yet, both ways */
}

/* All 8-bit arithmetic and logic results and flags, against a calculation made
 * another way (wide integers and signed values).
 *
 * Author: Thomas Dzubin */
static void test_alu_exhaustive(void)
{
    int op, a, m, cin, bad = 0;

    for (op = 0; op < 8; op++) {
        for (a = 0; a < 256; a++) {
            for (m = 0; m < 256; m++) {
                for (cin = 0; cin < ((op == 1 || op == 3) ? 2 : 1); cin++) {
                    int res, h, v, c, n, i, ones = 0;
                    uint8_t want_f, want_a;

                    power_on();
                    mem[0] = (uint8_t)(0x80 | (op << 3));       /* ALU op A,B */
                    cpu.a = (uint8_t)a;
                    cpu.b = (uint8_t)m;
                    cpu.f = cin ? Z80_F_C : 0;
                    cpuz80_step(&cpu);
                    if (op <= 1) {
                        int sum = a + m + cin, sv = (int8_t)a + (int8_t)m + cin;

                        res = sum & 0xFF;
                        h = ((a & 15) + (m & 15) + cin) > 15;
                        v = sv < -128 || sv > 127;
                        c = sum > 255;
                        n = 0;
                    } else if (op == 2 || op == 3 || op == 7) {
                        int diff = a - m - cin, sv = (int8_t)a - (int8_t)m - cin;

                        res = diff & 0xFF;
                        h = ((a & 15) - (m & 15) - cin) < 0;
                        v = sv < -128 || sv > 127;
                        c = diff < 0;
                        n = 1;
                    } else {
                        res = op == 4 ? (a & m) : op == 5 ? (a ^ m) : (a | m);
                        for (i = 0; i < 8; i++)
                            ones += (res >> i) & 1;
                        h = op == 4;
                        v = (ones % 2) == 0;
                        c = 0;
                        n = 0;
                    }
                    want_f = (uint8_t)((res & 0x80) | (res == 0 ? 0x40 : 0) |
                                       (op == 7 ? (m & 0x28) : (res & 0x28)) |
                                       (h ? 0x10 : 0) | (v ? 0x04 : 0) | (n ? 0x02 : 0) | (c ? 0x01 : 0));
                    want_a = op == 7 ? (uint8_t)a : (uint8_t)res;
                    if (cpu.a != want_a || cpu.f != want_f) {
                        if (bad++ < 5)
                            printf("   op %d a=%02X m=%02X cin=%d: A=%02X F=%02X, wanted A=%02X F=%02X\n",
                                   op, a, m, cin, cpu.a, cpu.f, want_a, want_f);
                    }
                }
            }
        }
    }
    CHECK(bad == 0);
}

/* INC and DEC of a register and of (HL), for every value and both carry states.
 *
 * Author: Thomas Dzubin */
static void test_inc_dec(void)
{
    int v, cin, bad = 0;

    for (v = 0; v < 256; v++) {
        for (cin = 0; cin < 2; cin++) {
            uint8_t r, f;

            power_on();                                 /* INC B, then DEC B */
            mem[0] = 0x04;
            mem[1] = 0x05;
            cpu.b = (uint8_t)v;
            cpu.f = cin ? Z80_F_C : 0;
            cpuz80_step(&cpu);
            r = (uint8_t)(v + 1);
            f = (uint8_t)((r & 0xA8) | (r == 0 ? 0x40 : 0) | (((v & 15) == 15) ? 0x10 : 0) |
                          (v == 0x7F ? 0x04 : 0) | (cin ? 1 : 0));
            if (cpu.b != r || cpu.f != f)
                bad++;
            cpu.b = (uint8_t)v;
            cpuz80_step(&cpu);
            r = (uint8_t)(v - 1);
            f = (uint8_t)((r & 0xA8) | (r == 0 ? 0x40 : 0) | (((v & 15) == 0) ? 0x10 : 0) |
                          (v == 0x80 ? 0x04 : 0) | 0x02 | (cin ? 1 : 0));
            if (cpu.b != r || cpu.f != f)
                bad++;
        }
    }
    CHECK(bad == 0);

    power_on();                                         /* INC (HL) */
    mem[0] = 0x34;
    cpu.h = 0x40;
    mem[0x4000] = 0x7F;
    CHECK(cpuz80_step(&cpu) == 11);
    CHECK(mem[0x4000] == 0x80 && (cpu.f & (Z80_F_S | Z80_F_PV | Z80_F_H)) == (Z80_F_S | Z80_F_PV | Z80_F_H));
}

/* Decimal adjust: every pair of two-digit BCD numbers added or subtracted gives
 * the BCD sum or difference.
 *
 * Author: Thomas Dzubin */
static void test_daa(void)
{
    int x, y, bad = 0;

    for (x = 0; x < 100; x++) {
        for (y = 0; y < 100; y++) {
            int sum = x + y, diff = x - y;
            uint8_t bx = (uint8_t)(((x / 10) << 4) | (x % 10));
            uint8_t by = (uint8_t)(((y / 10) << 4) | (y % 10));
            int s100 = sum % 100, d100 = (diff + 100) % 100;
            uint8_t want_add = (uint8_t)(((s100 / 10) << 4) | (s100 % 10));
            uint8_t want_sub = (uint8_t)(((d100 / 10) << 4) | (d100 % 10));

            power_on();                                 /* ADD A,B then DAA */
            mem[0] = 0x80;
            mem[1] = 0x27;
            cpu.a = bx;
            cpu.b = by;
            cpu.f = 0;
            run(2);
            if (cpu.a != want_add || ((cpu.f & Z80_F_C) != 0) != (sum > 99) ||
                ((cpu.f & Z80_F_Z) != 0) != (want_add == 0))
                bad++;
            power_on();                                 /* SUB B then DAA */
            mem[0] = 0x90;
            mem[1] = 0x27;
            cpu.a = bx;
            cpu.b = by;
            cpu.f = 0;
            run(2);
            if (cpu.a != want_sub || ((cpu.f & Z80_F_C) != 0) != (diff < 0) ||
                !(cpu.f & Z80_F_N))
                bad++;
        }
    }
    CHECK(bad == 0);
}

/* ADD HL,rr, the rotates of the accumulator, CPL, SCF and CCF.
 *
 * Author: Thomas Dzubin */
static void test_flag_instructions(void)
{
    power_on();                                         /* ADD HL,BC: carry out of bit 11 */
    mem[0] = 0x09;
    cpu.h = 0x0F; cpu.l = 0xFF;
    cpu.b = 0x00; cpu.c = 0x01;
    cpu.f = Z80_F_S | Z80_F_Z | Z80_F_PV | Z80_F_N;
    CHECK(cpuz80_step(&cpu) == 11);
    CHECK(hl() == 0x1000);
    CHECK((cpu.f & Z80_F_H) && !(cpu.f & Z80_F_C) && !(cpu.f & Z80_F_N));
    CHECK((cpu.f & (Z80_F_S | Z80_F_Z | Z80_F_PV)) == (Z80_F_S | Z80_F_Z | Z80_F_PV));   /* kept */

    power_on();                                         /* ADD HL,HL: carry out of bit 15 */
    mem[0] = 0x29;
    cpu.h = 0x80; cpu.l = 0x00;
    cpu.f = 0;
    run(1);
    CHECK(hl() == 0x0000 && (cpu.f & Z80_F_C) && (cpu.f & Z80_F_H) == 0);
    CHECK((cpu.f & Z80_F_XY) == 0);                     /* from the high byte of the result, 00H */

    power_on();                                         /* the undocumented bits follow H */
    mem[0] = 0x09;
    cpu.h = 0x27; cpu.l = 0x00;
    cpu.b = 0x01; cpu.c = 0x00;
    run(1);
    CHECK(hl() == 0x2800 && (cpu.f & Z80_F_XY) == Z80_F_XY);

    power_on();                                         /* RLCA, RRCA, RLA, RRA */
    mem[0] = 0x07; mem[1] = 0x0F; mem[2] = 0x17; mem[3] = 0x1F;
    cpu.a = 0x81;
    cpu.f = Z80_F_S | Z80_F_Z | Z80_F_PV | Z80_F_H | Z80_F_N;
    run(1);
    CHECK(cpu.a == 0x03 && (cpu.f & Z80_F_C) && !(cpu.f & (Z80_F_H | Z80_F_N)));
    CHECK((cpu.f & (Z80_F_S | Z80_F_Z | Z80_F_PV)) == (Z80_F_S | Z80_F_Z | Z80_F_PV));
    run(1);                                             /* 03H right, bit 0 into carry and bit 7 */
    CHECK(cpu.a == 0x81 && (cpu.f & Z80_F_C));
    run(1);                                             /* RLA: 81H left with carry in */
    CHECK(cpu.a == 0x03 && (cpu.f & Z80_F_C));
    run(1);                                             /* RRA: 03H right with carry in */
    CHECK(cpu.a == 0x81 && (cpu.f & Z80_F_C));

    power_on();                                         /* CPL, SCF, CCF */
    mem[0] = 0x2F; mem[1] = 0x37; mem[2] = 0x3F; mem[3] = 0x3F;
    cpu.a = 0x55;
    cpu.f = Z80_F_C;
    run(1);
    CHECK(cpu.a == 0xAA && (cpu.f & Z80_F_H) && (cpu.f & Z80_F_N) && (cpu.f & Z80_F_C));
    CHECK((cpu.f & Z80_F_XY) == (0xAA & Z80_F_XY));
    run(1);                                             /* SCF */
    CHECK((cpu.f & Z80_F_C) && !(cpu.f & (Z80_F_H | Z80_F_N)));
    run(1);                                             /* CCF: carry was 1 */
    CHECK(!(cpu.f & Z80_F_C) && (cpu.f & Z80_F_H));
    run(1);                                             /* CCF: carry was 0 */
    CHECK((cpu.f & Z80_F_C) && !(cpu.f & Z80_F_H));
}

/* Programs assembled with the project's assembler: loops, the stack, calls,
 * memory access, restarts.
 *
 * Author: Thomas Dzubin */
static void test_programs(void)
{
    int t;

    /* 5 + 4 + 3 + 2 + 1 in a loop */
    power_on();
    CHECK(load("LD B,5\n LD A,0\nloop: ADD A,B\n DEC B\n JP NZ,loop\n HALT\n") == 0);
    t = run_to_halt(100);
    CHECK(cpu.a == 15 && cpu.b == 0 && cpu.halted);
    CHECK(t == 7 + 7 + 5 * (4 + 4 + 10) + 4);           /* LD LD, five turns, HALT */
    CHECK(cpu.pc == 10);                                /* past the HALT */

    /* push, pop, call and return */
    power_on();
    CHECK(load("LD SP,8000H\n LD HL,1234H\n PUSH HL\n POP DE\n CALL sub\n HALT\nsub: LD A,77H\n RET\n") == 0);
    t = run_to_halt(100);
    CHECK(de() == 0x1234 && cpu.a == 0x77 && cpu.sp == 0x8000);
    CHECK(t == 10 + 10 + 11 + 10 + 17 + 7 + 10 + 4);

    /* EX (SP),HL and EX DE,HL */
    power_on();
    CHECK(load("LD SP,8000H\n LD HL,1111H\n PUSH HL\n LD HL,2222H\n EX (SP),HL\n LD DE,3333H\n EX DE,HL\n HALT\n") == 0);
    run_to_halt(100);
    CHECK(de() == 0x1111 && hl() == 0x3333 && mem[0x7FFE] == 0x22 && mem[0x7FFF] == 0x22);

    /* the loads and stores through memory */
    power_on();
    CHECK(load("LD HL,1234H\n LD (4000H),HL\n LD HL,0\n LD HL,(4000H)\n LD A,5AH\n LD (4010H),A\n LD A,0\n"
               " LD A,(4010H)\n LD BC,4020H\n LD (BC),A\n LD DE,4021H\n LD (DE),A\n LD A,0\n LD A,(DE)\n"
               " LD HL,4030H\n LD (HL),66H\n LD B,(HL)\n HALT\n") == 0);
    run_to_halt(100);
    CHECK(mem[0x4000] == 0x34 && mem[0x4001] == 0x12 && hl() == 0x4030);
    CHECK(mem[0x4010] == 0x5A && mem[0x4020] == 0x5A && mem[0x4021] == 0x5A && cpu.a == 0x5A);
    CHECK(mem[0x4030] == 0x66 && cpu.b == 0x66);

    /* conditional call and return: the call is not made, then it is */
    power_on();
    CHECK(load("LD SP,8000H\n XOR A\n CALL NZ,sub\n LD B,1\n CALL Z,sub\n HALT\nsub: INC C\n RET\n") == 0);
    run_to_halt(100);
    CHECK(cpu.b == 1 && cpu.c == 1);

    /* a restart, and the jump through HL */
    power_on();
    CHECK(load("LD SP,8000H\n RST 10H\n LD HL,there\n JP (HL)\n HALT\nthere: LD A,9\n HALT\n"
               "ORG 10H\n LD D,7\n RET\n") == 0);
    run_to_halt(100);
    CHECK(cpu.d == 7 && cpu.a == 9);
}

/* IN A,(n) and OUT (n),A put the accumulator on the high half of the address bus.
 * The instructions are the 8080's; their port decoding is the Z80's own. */
static void test_io(void)
{
    power_on();
    CHECK(load("LD A,12H\n OUT (34H),A\n LD A,56H\n IN A,(78H)\n HALT\n") == 0);
    in_value = 0xC3;
    run(2);
    CHECK(io_writes == 1 && last_port == 0x1234 && last_out == 0x12);
    run(2);
    CHECK(io_reads == 1 && last_port == 0x5678 && cpu.a == 0xC3);
}

/* Reset, the interrupt modes, EI's one instruction delay, HALT, NMI.
 *
 * Author: Thomas Dzubin */
static void test_interrupts(void)
{
    int t;

    power_on();
    CHECK(cpu.pc == 0 && cpu.a == 0xFF && cpu.f == 0xFF && !cpu.iff1 && !cpu.iff2 && cpu.im == 0);
    mem[0x38] = 0x00;

    /* mode 1: EI does not let an interrupt in until the next instruction has run */
    CHECK(load("EI\n NOP\n NOP\n") == 0);
    cpu.im = 1;
    cpu.irq = true;
    CHECK(cpuz80_step(&cpu) == 4 && cpu.iff1 && cpu.iff2);           /* EI */
    CHECK(cpuz80_step(&cpu) == 4 && cpu.pc == 2);                    /* NOP, still no interrupt */
    t = cpuz80_step(&cpu);                                           /* now it is taken */
    CHECK(t == 13 && cpu.pc == 0x38 && !cpu.iff1 && !cpu.iff2);
    CHECK(cpu.sp == 0x7FFE && mem[0x7FFE] == 0x02 && mem[0x7FFF] == 0x00);

    /* DI keeps it out */
    power_on();
    CHECK(load("DI\n NOP\n NOP\n") == 0);
    cpu.im = 1;
    cpu.irq = true;
    run(3);
    CHECK(cpu.pc == 3);

    /* mode 2: the vector table is at I * 256 + the byte from the device */
    power_on();
    cpu.iff1 = cpu.iff2 = true;
    cpu.im = 2;
    cpu.i = 0x40;
    cpu.irq = true;
    cpu.irq_data = 0x11;                                /* bit 0 is ignored */
    mem[0x4010] = 0x78;
    mem[0x4011] = 0x56;
    CHECK(cpuz80_step(&cpu) == 19 && cpu.pc == 0x5678);

    /* mode 0: a restart from the bus */
    power_on();
    cpu.iff1 = cpu.iff2 = true;
    cpu.im = 0;
    cpu.irq = true;
    cpu.irq_data = 0xDF;                                /* RST 18H */
    CHECK(cpuz80_step(&cpu) == 13 && cpu.pc == 0x18);

    /* NMI: always taken, to 66H, and RETN would restore the enable from IFF2 */
    power_on();
    cpu.iff1 = true;
    cpu.iff2 = false;
    mem[0] = 0x00;
    cpuz80_nmi(&cpu, true);
    CHECK(cpuz80_step(&cpu) == 11 && cpu.pc == 0x66 && !cpu.iff1 && cpu.iff2);
    CHECK(mem[0x7FFE] == 0x00 && mem[0x7FFF] == 0x00);

    /* HALT runs NOPs until an interrupt, which returns to the instruction after it */
    power_on();
    CHECK(load("EI\n HALT\n NOP\n") == 0);
    cpu.im = 1;
    run(2);
    CHECK(cpu.halted && cpu.pc == 2);
    CHECK(cpuz80_step(&cpu) == 4 && cpu.pc == 2 && cpu.halted);
    cpu.irq = true;
    CHECK(cpuz80_step(&cpu) == 13 && !cpu.halted && cpu.pc == 0x38);
    CHECK(mem[0x7FFE] == 0x02 && mem[0x7FFF] == 0x00);
}

/* R counts opcode fetches in its low seven bits and keeps bit 7. */
static void test_refresh(void)
{
    power_on();
    CHECK(cpu.r == 0);
    cpu.r = 0x80;
    run(3);                                             /* three NOPs in the cleared memory */
    CHECK(cpu.r == 0x83);
    cpu.r = 0x7F;
    run(1);
    CHECK(cpu.r == 0x00);
    cpu.r = 0xFF;
    run(1);
    CHECK(cpu.r == 0x80);
}

/* An opcode that is not run yet is flagged for one instruction only. */
static void test_unsupported(void)
{
    power_on();
    mem[0] = 0x18;                                      /* JR */
    mem[1] = 0x00;                                      /* (a NOP: the core takes JR as one byte) */
    mem[2] = 0x00;
    cpuz80_step(&cpu);
    CHECK(cpu.unsupported && cpu.unsupported_op == 0x18 && cpu.pc == 1);
    cpuz80_step(&cpu);                                  /* the next one is a NOP */
    CHECK(!cpu.unsupported && cpu.pc == 2);
    cpu.pc = 0;
    mem[0] = 0xCB;
    cpuz80_step(&cpu);
    CHECK(cpu.unsupported && cpu.unsupported_op == 0xCB);
}

int main(void)
{
    test_opcode_table();
    test_alu_exhaustive();
    test_inc_dec();
    test_daa();
    test_flag_instructions();
    test_programs();
    test_io();
    test_interrupts();
    test_refresh();
    test_unsupported();
    printf("%d checks, %d failed\n", checks, failures);
    return failures != 0;
}
