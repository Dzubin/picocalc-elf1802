/*
 * cpuz80.c - the Zilog Z80 CPU core. See cpuz80.h.
 *
 * An opcode is read as bit fields: x = bits 7-6, y = bits 5-3, z = bits 2-0 and
 * y split in p = y / 2 and q = y mod 2 (the same fields isa_z80.c decodes by).
 * The registers by their number in the opcode are B C D E H L (HL) A, the pairs
 * BC DE HL SP (AF in place of SP for PUSH and POP).
 *
 * Author: Thomas Dzubin
 */
#include <string.h>

#include "cpuz80.h"

static const uint8_t t_states[256] = CPUZ80_TSTATES;

/* ---------------------------------------------------------------------- */
/*  Memory, the registers and the stack                                    */
/* ---------------------------------------------------------------------- */

static uint8_t rd(cpuz80_t *cpu, uint16_t addr)
{
    return cpu->mem_read ? cpu->mem_read(cpu->ctx, addr) : Z80_OPEN_BUS;
}

static void wr(cpuz80_t *cpu, uint16_t addr, uint8_t data)
{
    if (cpu->mem_write)
        cpu->mem_write(cpu->ctx, addr, data);
}

static uint16_t rd16(cpuz80_t *cpu, uint16_t addr)
{
    uint16_t lo = rd(cpu, addr);

    return (uint16_t)(lo | (rd(cpu, (uint16_t)(addr + 1)) << 8));
}

static void wr16(cpuz80_t *cpu, uint16_t addr, uint16_t data)
{
    wr(cpu, addr, (uint8_t)data);
    wr(cpu, (uint16_t)(addr + 1), (uint8_t)(data >> 8));
}

static uint8_t fetch8(cpuz80_t *cpu)
{
    return rd(cpu, cpu->pc++);
}

static uint16_t fetch16(cpuz80_t *cpu)
{
    uint16_t lo = fetch8(cpu);

    return (uint16_t)(lo | (fetch8(cpu) << 8));
}

/* R counts the opcode fetches in its low seven bits; bit 7 is only what a program
 * stored in it. */
static void bump_r(cpuz80_t *cpu)
{
    cpu->r = (uint8_t)((cpu->r & ~Z80_R_MASK) | ((cpu->r + 1) & Z80_R_MASK));
}

static void push16(cpuz80_t *cpu, uint16_t v)
{
    wr(cpu, --cpu->sp, (uint8_t)(v >> 8));
    wr(cpu, --cpu->sp, (uint8_t)v);
}

static uint16_t pop16(cpuz80_t *cpu)
{
    uint16_t lo = rd(cpu, cpu->sp++);

    return (uint16_t)(lo | (rd(cpu, cpu->sp++) << 8));
}

static uint16_t hl_of(const cpuz80_t *cpu)
{
    return (uint16_t)((cpu->h << 8) | cpu->l);
}

static void set_hl(cpuz80_t *cpu, uint16_t v)
{
    cpu->h = (uint8_t)(v >> 8);
    cpu->l = (uint8_t)v;
}

/* The register with this number in the opcode (not 6, which is (HL)). */
static uint8_t *reg_ptr(cpuz80_t *cpu, int r)
{
    switch (r) {
    case 0:  return &cpu->b;
    case 1:  return &cpu->c;
    case 2:  return &cpu->d;
    case 3:  return &cpu->e;
    case 4:  return &cpu->h;
    case 5:  return &cpu->l;
    default: return &cpu->a;
    }
}

static uint8_t get_r(cpuz80_t *cpu, int r)
{
    return r == Z80_REG_HL_INDIRECT ? rd(cpu, hl_of(cpu)) : *reg_ptr(cpu, r);
}

static void set_r(cpuz80_t *cpu, int r, uint8_t v)
{
    if (r == Z80_REG_HL_INDIRECT)
        wr(cpu, hl_of(cpu), v);
    else
        *reg_ptr(cpu, r) = v;
}

/* A register pair by its number; af says whether number 3 is AF (PUSH, POP) or SP. */
static uint16_t get_rp(const cpuz80_t *cpu, int p, bool af)
{
    switch (p) {
    case 0:  return (uint16_t)((cpu->b << 8) | cpu->c);
    case 1:  return (uint16_t)((cpu->d << 8) | cpu->e);
    case 2:  return hl_of(cpu);
    default: return af ? (uint16_t)((cpu->a << 8) | cpu->f) : cpu->sp;
    }
}

/* Author: Thomas Dzubin */
static void set_rp(cpuz80_t *cpu, int p, uint16_t v, bool af)
{
    switch (p) {
    case 0:
        cpu->b = (uint8_t)(v >> 8);
        cpu->c = (uint8_t)v;
        break;
    case 1:
        cpu->d = (uint8_t)(v >> 8);
        cpu->e = (uint8_t)v;
        break;
    case 2:
        set_hl(cpu, v);
        break;
    default:
        if (af) {
            cpu->a = (uint8_t)(v >> 8);
            cpu->f = (uint8_t)v;
        } else {
            cpu->sp = v;
        }
        break;
    }
}

/* ---------------------------------------------------------------------- */
/*  Flags and arithmetic                                                   */
/* ---------------------------------------------------------------------- */

/* S, Z and the two undocumented bits from an 8-bit result. */
static uint8_t szxy(uint8_t r)
{
    return (uint8_t)((r & (Z80_F_S | Z80_F_XY)) | (r == 0 ? Z80_F_Z : 0));
}

/* PV set for even parity. */
static uint8_t parity(uint8_t v)
{
    v ^= (uint8_t)(v >> 4);
    v ^= (uint8_t)(v >> 2);
    v ^= (uint8_t)(v >> 1);
    return (v & 1) ? 0 : Z80_F_PV;
}

/* The eight arithmetic and logic operations on the accumulator, by their number
 * in the opcode: ADD ADC SUB SBC AND XOR OR CP. Sets the flags and returns the
 * result (for CP the caller does not store it).
 *
 * Author: Thomas Dzubin */
static uint8_t alu(cpuz80_t *cpu, int op, uint8_t a, uint8_t m)
{
    unsigned carry = ((op == Z80_ALU_ADC || op == Z80_ALU_SBC) && (cpu->f & Z80_F_C)) ? 1u : 0u;
    unsigned res;
    uint8_t f, r8;

    switch (op) {
    case Z80_ALU_ADD:
    case Z80_ALU_ADC:
        res = (unsigned)a + m + carry;
        r8 = (uint8_t)res;
        f = (uint8_t)(szxy(r8) | ((a ^ m ^ r8) & Z80_F_H) |
                      ((~(a ^ m) & (a ^ r8) & Z80_SIGN8) ? Z80_F_PV : 0) |
                      (res > 0xFFu ? Z80_F_C : 0));
        break;
    case Z80_ALU_SUB:
    case Z80_ALU_SBC:
    case Z80_ALU_CP:
        res = (unsigned)a - m - carry;
        r8 = (uint8_t)res;
        f = (uint8_t)(szxy(r8) | Z80_F_N | ((a ^ m ^ r8) & Z80_F_H) |
                      (((a ^ m) & (a ^ r8) & Z80_SIGN8) ? Z80_F_PV : 0) |
                      ((unsigned)a < (unsigned)m + carry ? Z80_F_C : 0));
        if (op == Z80_ALU_CP)                       /* the undocumented bits are the operand's */
            f = (uint8_t)((f & ~Z80_F_XY) | (m & Z80_F_XY));
        break;
    case Z80_ALU_AND:
        r8 = (uint8_t)(a & m);
        f = (uint8_t)(szxy(r8) | Z80_F_H | parity(r8));
        break;
    case Z80_ALU_XOR:
        r8 = (uint8_t)(a ^ m);
        f = (uint8_t)(szxy(r8) | parity(r8));
        break;
    default:                                        /* OR */
        r8 = (uint8_t)(a | m);
        f = (uint8_t)(szxy(r8) | parity(r8));
        break;
    }
    cpu->f = f;
    return r8;
}

static uint8_t inc8(cpuz80_t *cpu, uint8_t v)
{
    uint8_t r = (uint8_t)(v + 1);

    cpu->f = (uint8_t)((cpu->f & Z80_F_C) | szxy(r) | ((v & 0x0F) == 0x0F ? Z80_F_H : 0) |
                       (v == Z80_SIGN8 - 1 ? Z80_F_PV : 0));
    return r;
}

static uint8_t dec8(cpuz80_t *cpu, uint8_t v)
{
    uint8_t r = (uint8_t)(v - 1);

    cpu->f = (uint8_t)((cpu->f & Z80_F_C) | szxy(r) | Z80_F_N | ((v & 0x0F) == 0 ? Z80_F_H : 0) |
                       (v == Z80_SIGN8 ? Z80_F_PV : 0));
    return r;
}

/* ADD HL,rr: carry and half carry from bits 15 and 11, S, Z and PV unchanged. */
static void add_hl(cpuz80_t *cpu, uint16_t v)
{
    unsigned hl = hl_of(cpu);
    unsigned res = hl + v;

    cpu->f = (uint8_t)((cpu->f & (Z80_F_S | Z80_F_Z | Z80_F_PV)) | ((res >> 8) & Z80_F_XY) |
                       (((hl ^ v ^ res) & Z80_HALF_BIT16) ? Z80_F_H : 0) |
                       (res > 0xFFFFu ? Z80_F_C : 0));
    set_hl(cpu, (uint16_t)res);
}

/* RLCA, RRCA, RLA, RRA (y = 0 to 3 of the accumulator group). */
static void rotate_a(cpuz80_t *cpu, int y)
{
    uint8_t a = cpu->a, c;

    switch (y) {
    case 0:  c = (uint8_t)(a >> 7); a = (uint8_t)((a << 1) | c); break;
    case 1:  c = (uint8_t)(a & 1);  a = (uint8_t)((a >> 1) | (c << 7)); break;
    case 2:  c = (uint8_t)(a >> 7); a = (uint8_t)((a << 1) | (cpu->f & Z80_F_C)); break;
    default: c = (uint8_t)(a & 1);  a = (uint8_t)((a >> 1) | ((cpu->f & Z80_F_C) << 7)); break;
    }
    cpu->a = a;
    cpu->f = (uint8_t)((cpu->f & (Z80_F_S | Z80_F_Z | Z80_F_PV)) | (a & Z80_F_XY) | c);
}

/* Decimal adjust, for the result of an addition or (with N set) a subtraction
 * of two BCD numbers.
 *
 * Author: Thomas Dzubin */
static void daa(cpuz80_t *cpu)
{
    uint8_t a = cpu->a, correction = 0, f = cpu->f;
    bool carry = (f & Z80_F_C) != 0;

    if ((f & Z80_F_H) || (a & 0x0F) > 9)
        correction |= 0x06;
    if (carry || a > 0x99) {
        correction |= 0x60;
        carry = true;
    }
    if (f & Z80_F_N) {
        cpu->f = (uint8_t)((((f & Z80_F_H) && (a & 0x0F) < 6) ? Z80_F_H : 0));
        a = (uint8_t)(a - correction);
    } else {
        cpu->f = (uint8_t)(((a & 0x0F) > 9) ? Z80_F_H : 0);
        a = (uint8_t)(a + correction);
    }
    cpu->f = (uint8_t)(cpu->f | szxy(a) | parity(a) | (f & Z80_F_N) | (carry ? Z80_F_C : 0));
    cpu->a = a;
}

/* The accumulator group's single flag instructions: CPL, SCF and CCF. */
static void flag_op(cpuz80_t *cpu, int y)
{
    uint8_t keep = (uint8_t)(cpu->f & (Z80_F_S | Z80_F_Z | Z80_F_PV));
    bool carry = (cpu->f & Z80_F_C) != 0;

    if (y == 5) {                                   /* CPL */
        cpu->a = (uint8_t)~cpu->a;
        cpu->f = (uint8_t)(keep | (cpu->f & Z80_F_C) | Z80_F_H | Z80_F_N | (cpu->a & Z80_F_XY));
    } else if (y == 6) {                            /* SCF */
        cpu->f = (uint8_t)(keep | (cpu->a & Z80_F_XY) | Z80_F_C);
    } else {                                        /* CCF: H is the carry it had */
        cpu->f = (uint8_t)(keep | (cpu->a & Z80_F_XY) | (carry ? Z80_F_H : Z80_F_C));
    }
}

/* Is the condition (NZ Z NC C PO PE P M) true? */
static bool condition(const cpuz80_t *cpu, int cc)
{
    switch (cc) {
    case Z80_COND_NZ: return !(cpu->f & Z80_F_Z);
    case Z80_COND_Z:  return (cpu->f & Z80_F_Z) != 0;
    case Z80_COND_NC: return !(cpu->f & Z80_F_C);
    case Z80_COND_C:  return (cpu->f & Z80_F_C) != 0;
    case Z80_COND_PO: return !(cpu->f & Z80_F_PV);
    case Z80_COND_PE: return (cpu->f & Z80_F_PV) != 0;
    case Z80_COND_P:  return !(cpu->f & Z80_F_S);
    default:          return (cpu->f & Z80_F_S) != 0;
    }
}

/* ---------------------------------------------------------------------- */
/*  Interrupts and reset                                                   */
/* ---------------------------------------------------------------------- */

/* Take an interrupt if one is wanted; returns the T-states it took or 0. A
 * maskable interrupt waits while EI's delay lasts (allow_irq is false).
 *
 * Author: Thomas Dzubin */
static int take_interrupt(cpuz80_t *cpu, bool allow_irq)
{
    if (cpu->nmi_pending) {
        cpu->nmi_pending = false;
        cpu->halted = false;
        cpu->iff2 = cpu->iff1;                      /* RETN gives the enable back */
        cpu->iff1 = false;
        bump_r(cpu);
        push16(cpu, cpu->pc);
        cpu->pc = Z80_NMI_ADDRESS;
        return Z80_T_NMI;
    }
    if (!allow_irq || !cpu->irq || !cpu->iff1)
        return 0;
    cpu->halted = false;
    cpu->iff1 = cpu->iff2 = false;
    bump_r(cpu);
    push16(cpu, cpu->pc);
    if (cpu->im == 2) {
        cpu->pc = rd16(cpu, (uint16_t)((cpu->i << 8) | (cpu->irq_data & Z80_VECTOR_MASK)));
        return Z80_T_IRQ_MODE2;
    }
    cpu->pc = cpu->im == 1 ? Z80_IRQ_ADDRESS : (uint16_t)(cpu->irq_data & Z80_RST_MASK);
    return Z80_T_IRQ_RST;
}

void cpuz80_nmi(cpuz80_t *cpu, bool asserted)
{
    if (asserted)
        cpu->nmi_pending = true;
}

/* Author: Thomas Dzubin */
void cpuz80_reset(cpuz80_t *cpu)
{
    cpu->a = (uint8_t)(Z80_RESET_AF >> 8);
    cpu->f = (uint8_t)Z80_RESET_AF;
    cpu->b = cpu->c = cpu->d = cpu->e = cpu->h = cpu->l = 0;
    cpu->a2 = cpu->f2 = cpu->b2 = cpu->c2 = cpu->d2 = cpu->e2 = cpu->h2 = cpu->l2 = 0;
    cpu->ix = cpu->iy = 0;
    cpu->sp = Z80_RESET_SP;
    cpu->pc = Z80_RESET_PC;
    cpu->i = cpu->r = 0;
    cpu->iff1 = cpu->iff2 = false;
    cpu->im = 0;
    cpu->halted = false;
    cpu->ei_delay = false;
    cpu->irq = false;
    cpu->irq_data = Z80_IRQ_DATA_IDLE;
    cpu->nmi_pending = false;
    cpu->unsupported = false;
    cpu->unsupported_op = 0;
    cpu->cycles = 0;
}

/* ---------------------------------------------------------------------- */
/*  One instruction                                                        */
/* ---------------------------------------------------------------------- */

/* The unprefixed opcodes 0x00 to 0x3F. Returns false for one this core does not
 * run yet.
 *
 * Author: Thomas Dzubin */
static bool execute_x0(cpuz80_t *cpu, uint8_t op)
{
    int y = (op >> 3) & 7, p = y >> 1, q = y & 1;
    uint16_t nn;

    switch (op & 7) {
    case 0:
        return op == 0x00;                          /* NOP; JR, DJNZ and EX AF,AF' are not run yet */
    case 1:
        if (q)
            add_hl(cpu, get_rp(cpu, p, false));
        else
            set_rp(cpu, p, fetch16(cpu), false);
        return true;
    case 2:
        if (p == 2) {                               /* LD (nn),HL and LD HL,(nn) */
            nn = fetch16(cpu);
            if (q)
                set_hl(cpu, rd16(cpu, nn));
            else
                wr16(cpu, nn, hl_of(cpu));
        } else if (p == 3) {                        /* LD (nn),A and LD A,(nn) */
            nn = fetch16(cpu);
            if (q)
                cpu->a = rd(cpu, nn);
            else
                wr(cpu, nn, cpu->a);
        } else if (q) {                             /* LD A,(BC) and LD A,(DE) */
            cpu->a = rd(cpu, get_rp(cpu, p, false));
        } else {
            wr(cpu, get_rp(cpu, p, false), cpu->a);
        }
        return true;
    case 3:
        set_rp(cpu, p, (uint16_t)(get_rp(cpu, p, false) + (q ? -1 : 1)), false);
        return true;
    case 4:
        set_r(cpu, y, inc8(cpu, get_r(cpu, y)));
        return true;
    case 5:
        set_r(cpu, y, dec8(cpu, get_r(cpu, y)));
        return true;
    case 6:
        set_r(cpu, y, fetch8(cpu));
        return true;
    default:                                        /* the accumulator group */
        if (y < 4)
            rotate_a(cpu, y);
        else if (y == 4)
            daa(cpu);
        else
            flag_op(cpu, y);
        return true;
    }
}

/* The unprefixed opcodes 0xC0 to 0xFF; returns the T-states beyond the table's
 * (a taken conditional RET or CALL), or -1 for an opcode this core does not run
 * yet.
 *
 * Author: Thomas Dzubin */
static int execute_x3(cpuz80_t *cpu, uint8_t op)
{
    int y = (op >> 3) & 7, p = y >> 1, q = y & 1;
    uint16_t nn, v;
    uint8_t n;

    switch (op & 7) {
    case 0:                                         /* RET cc */
        if (!condition(cpu, y))
            return 0;
        cpu->pc = pop16(cpu);
        return Z80_T_RET_TAKEN;
    case 1:
        if (!q)
            set_rp(cpu, p, pop16(cpu), true);
        else if (p == 0)
            cpu->pc = pop16(cpu);
        else if (p == 2)
            cpu->pc = hl_of(cpu);
        else if (p == 3)
            cpu->sp = hl_of(cpu);
        else
            return -1;                              /* EXX */
        return 0;
    case 2:                                         /* JP cc,nn */
        nn = fetch16(cpu);
        if (condition(cpu, y))
            cpu->pc = nn;
        return 0;
    case 3:
        switch (y) {
        case 0:
            cpu->pc = fetch16(cpu);
            return 0;
        case 2:                                     /* OUT (n),A */
            n = fetch8(cpu);
            if (cpu->io_out)
                cpu->io_out(cpu->ctx, (uint16_t)((cpu->a << 8) | n), cpu->a);
            return 0;
        case 3:                                     /* IN A,(n) */
            n = fetch8(cpu);
            cpu->a = cpu->io_in ? cpu->io_in(cpu->ctx, (uint16_t)((cpu->a << 8) | n)) : Z80_OPEN_BUS;
            return 0;
        case 4:                                     /* EX (SP),HL */
            v = rd16(cpu, cpu->sp);
            wr16(cpu, cpu->sp, hl_of(cpu));
            set_hl(cpu, v);
            return 0;
        case 5:                                     /* EX DE,HL */
            v = get_rp(cpu, 1, false);
            set_rp(cpu, 1, hl_of(cpu), false);
            set_hl(cpu, v);
            return 0;
        case 6:                                     /* DI */
            cpu->iff1 = cpu->iff2 = false;
            return 0;
        case 7:                                     /* EI: the next instruction runs first */
            cpu->iff1 = cpu->iff2 = true;
            cpu->ei_delay = true;
            return 0;
        default:
            return -1;                              /* the CB prefix */
        }
    case 4:                                         /* CALL cc,nn */
        nn = fetch16(cpu);
        if (!condition(cpu, y))
            return 0;
        push16(cpu, cpu->pc);
        cpu->pc = nn;
        return Z80_T_CALL_TAKEN;
    case 5:
        if (!q) {
            push16(cpu, get_rp(cpu, p, true));
            return 0;
        }
        if (p != 0)
            return -1;                              /* the DD, ED and FD prefixes */
        nn = fetch16(cpu);
        push16(cpu, cpu->pc);
        cpu->pc = nn;
        return 0;
    case 6: {                                       /* ADD A,n and the rest */
        uint8_t r = alu(cpu, y, cpu->a, fetch8(cpu));

        if (y != Z80_ALU_CP)
            cpu->a = r;
        return 0;
    }
    default:                                        /* RST */
        push16(cpu, cpu->pc);
        cpu->pc = (uint16_t)(op & Z80_RST_MASK);
        return 0;
    }
}

/* Run the opcode just fetched; returns its T-states.
 *
 * Author: Thomas Dzubin */
static int execute(cpuz80_t *cpu, uint8_t op)
{
    int x = op >> 6, y = (op >> 3) & 7, z = op & 7, extra = 0;
    bool ok = true;

    switch (x) {
    case 0:
        ok = execute_x0(cpu, op);
        break;
    case 1:
        if (op == Z80_OP_HALT)
            cpu->halted = true;
        else
            set_r(cpu, y, get_r(cpu, z));
        break;
    case 2: {
        uint8_t r = alu(cpu, y, cpu->a, get_r(cpu, z));

        if (y != Z80_ALU_CP)
            cpu->a = r;
        break;
    }
    default:
        extra = execute_x3(cpu, op);
        ok = extra >= 0;
        break;
    }
    if (!ok) {                                      /* not run yet: a one-byte NOP */
        cpu->unsupported = true;
        cpu->unsupported_op = op;
        return Z80_T_UNSUPPORTED;
    }
    return t_states[op] + extra;
}

int cpuz80_step(cpuz80_t *cpu)
{
    bool allow_irq = !cpu->ei_delay;
    int t;

    cpu->ei_delay = false;
    cpu->unsupported = false;
    t = take_interrupt(cpu, allow_irq);
    if (t == 0) {
        if (cpu->halted) {
            bump_r(cpu);
            t = Z80_T_HALTED;
        } else {
            bump_r(cpu);
            t = execute(cpu, fetch8(cpu));
        }
    }
    cpu->cycles += (uint32_t)t;
    return t;
}
