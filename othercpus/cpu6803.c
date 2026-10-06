/*
 * cpu6803.c - the Motorola MC6803. See cpu6803.h.
 *
 * The instruction set is the 6800's with the 6801 additions (LDD, STD, ADDD,
 * SUBD, ABX, MUL, PSHX, PULX, LSRD, ASLD, BRN, JSR direct). The cycle counts
 * are the data sheet's. Condition codes follow the data sheet: CPX sets C on
 * the 6803 (unlike the 6800), and the illegal opcodes take the trap vector.
 *
 * Author: Thomas Dzubin
 */
#include <string.h>

#include "cpu6803.h"

static const uint8_t cycle_table[256] = CPU6803_CYCLES;
#undef XX

/* ---------------------------------------------------------------------- */
/*  The on-chip timer                                                      */
/* ---------------------------------------------------------------------- */

static uint8_t port_pins(const cpu6803_t *c, int p)
{
    uint8_t d = c->ddr[p];

    return (uint8_t)((c->data[p] & d) | (uint8_t)~d);
}

static void port_write(cpu6803_t *c, int p)
{
    uint8_t pins = port_pins(c, p);
    uint8_t d = c->ddr[p];

    if (p == 1) {                   /* only five bits of port 2 are pins */
        pins &= M68_P2_MASK;
        d &= M68_P2_MASK;
    }
    if (c->port_out)
        c->port_out(c->ctx, p + 1, pins, d);
}

static void compare_match(cpu6803_t *c)
{
    c->tcsr |= M68_TCSR_OCF;
    c->pending |= M68_TCSR_OCF;
    if (c->ddr[1] & M68_P2_OC_BIT) {
        c->data[1] = (uint8_t)((c->data[1] & ~M68_P2_OC_BIT) |
                               ((c->tcsr & M68_TCSR_OLVL) << 1));
        port_write(c, 1);
    }
}

/* Advance the counter by n E clocks (n is small: an instruction or an interrupt
 * entry). The compare value and $FFFF are each passed at most once in that time,
 * so how far away they are says whether this stretch of clocks reaches them; it
 * gives what counting one clock at a time does. */
static void tick(cpu6803_t *c, int n)
{
    uint16_t to_compare = (uint16_t)(c->ocr - c->counter);
    uint16_t to_full = (uint16_t)(M68_COUNTER_FULL - c->counter);

    c->cycles += (uint32_t)n;
    c->counter = (uint16_t)(c->counter + n);
    if (to_compare >= 1 && to_compare <= n)
        compare_match(c);
    if (to_full >= 1 && to_full <= n) {
        c->tcsr |= M68_TCSR_TOF;
        c->pending |= M68_TCSR_TOF;
    }
}

void cpu6803_input_capture(cpu6803_t *c, bool level)
{
    bool edge = level != c->tin;

    c->tin = level;
    /* an edge of the direction TCSR selects */
    if (edge && level == ((c->tcsr & M68_TCSR_IEDG) != 0)) {
        c->tcsr |= M68_TCSR_ICF;
        c->pending |= M68_TCSR_ICF;
        c->icr = c->counter;
    }
}

/* ---------------------------------------------------------------------- */
/*  Memory as the CPU sees it                                              */
/* ---------------------------------------------------------------------- */

static uint8_t port_read(cpu6803_t *c, int p)
{
    uint8_t d = c->ddr[p];

    if (d == 0xFF)
        return c->data[p];
    return (uint8_t)(((c->port_in ? c->port_in(c->ctx, p + 1) : M68_OPEN_BUS) & ~d) |
                     (c->data[p] & d));
}

/* Author: Thomas Dzubin */
static uint8_t reg_read(cpu6803_t *c, uint16_t a)
{
    uint8_t v;

    switch (a) {
    case M68_REG_P1:    return port_read(c, 0);
    case M68_REG_P2:    return port_read(c, 1);
    case M68_REG_P3:    return port_read(c, 2);
    case M68_REG_P4:    return port_read(c, 3);
    case M68_REG_TCSR:
        c->pending = 0;
        return c->tcsr;
    case M68_REG_CNT_H:
        if (!(c->pending & M68_TCSR_TOF))
            c->tcsr &= (uint8_t)~M68_TCSR_TOF;
        c->counter_low = (uint8_t)c->counter;
        c->counter_low_held = true;
        return (uint8_t)(c->counter >> 8);
    case M68_REG_CNT_L:
        v = c->counter_low_held ? c->counter_low : (uint8_t)c->counter;
        c->counter_low_held = false;
        return v;
    case M68_REG_OCR_H: return (uint8_t)(c->ocr >> 8);
    case M68_REG_OCR_L: return (uint8_t)c->ocr;
    case M68_REG_ICR_H:
        if (!(c->pending & M68_TCSR_ICF))
            c->tcsr &= (uint8_t)~M68_TCSR_ICF;
        return (uint8_t)(c->icr >> 8);
    case M68_REG_ICR_L: return (uint8_t)c->icr;
    case M68_REG_P3CSR: return c->p3csr;
    case M68_REG_RMCR:  return c->rmcr;
    case M68_REG_TRCSR: return c->trcsr;
    case M68_REG_RDR:   return c->rdr;
    case M68_REG_RCR:   return (uint8_t)(M68_RCR_ONES | (c->rcr & M68_RCR_STORED));
    default:            return M68_OPEN_BUS;    /* the direction registers, TDR */
    }
}

/* Author: Thomas Dzubin */
static void reg_write(cpu6803_t *c, uint16_t a, uint8_t v)
{
    switch (a) {
    case M68_REG_DDR1: c->ddr[0] = v; port_write(c, 0); break;
    case M68_REG_DDR2: c->ddr[1] = v; port_write(c, 1); break;
    case M68_REG_DDR3: c->ddr[2] = v; port_write(c, 2); break;
    case M68_REG_DDR4: c->ddr[3] = v; port_write(c, 3); break;
    case M68_REG_P1:   c->data[0] = v; port_write(c, 0); break;
    case M68_REG_P2:   c->data[1] = v; port_write(c, 1); break;
    case M68_REG_P3:   c->data[2] = v; port_write(c, 2); break;
    case M68_REG_P4:   c->data[3] = v; port_write(c, 3); break;
    case M68_REG_TCSR:
        c->tcsr = (uint8_t)((v & M68_TCSR_WRITE) | (c->tcsr & M68_TCSR_FLAGS));
        c->pending &= c->tcsr;
        break;
    case M68_REG_CNT_H:
        c->counter_low = v;                 /* held for the low byte write */
        c->counter = M68_COUNTER_PRESET;
        break;
    case M68_REG_CNT_L:
        c->counter = (uint16_t)((c->counter_low << 8) | v);
        break;
    case M68_REG_OCR_H:
        if (!(c->pending & M68_TCSR_OCF))
            c->tcsr &= (uint8_t)~M68_TCSR_OCF;
        c->ocr = (uint16_t)((c->ocr & 0x00FF) | (v << 8));
        break;
    case M68_REG_OCR_L:
        if (!(c->pending & M68_TCSR_OCF))
            c->tcsr &= (uint8_t)~M68_TCSR_OCF;
        c->ocr = (uint16_t)((c->ocr & 0xFF00) | v);
        break;
    case M68_REG_P3CSR: c->p3csr = v; break;
    case M68_REG_RMCR:  c->rmcr = v; break;
    case M68_REG_TRCSR:
        c->trcsr = (uint8_t)((v & M68_TRCSR_WRITE) | M68_TRCSR_TDRE);
        break;
    case M68_REG_RCR:
        c->rcr = v;
        c->ram_enabled = (v & M68_RCR_RAME) != 0;
        break;
    default:            break;
    }
}

static uint8_t rd(cpu6803_t *c, uint16_t a)
{
    if (a <= M68_REG_LAST)
        return reg_read(c, a);
    if (a >= M68_RAM_BASE && a < M68_RAM_BASE + M68_RAM_SIZE && c->ram_enabled)
        return c->ram[a - M68_RAM_BASE];
    return c->mem_read ? c->mem_read(c->ctx, a) : M68_OPEN_BUS;
}

static void wr(cpu6803_t *c, uint16_t a, uint8_t v)
{
    if (a <= M68_REG_LAST) {
        reg_write(c, a, v);
        return;
    }
    if (a >= M68_RAM_BASE && a < M68_RAM_BASE + M68_RAM_SIZE && c->ram_enabled) {
        c->ram[a - M68_RAM_BASE] = v;
        return;
    }
    if (c->mem_write)
        c->mem_write(c->ctx, a, v);
}

static uint16_t rd16(cpu6803_t *c, uint16_t a)
{
    uint16_t hi = rd(c, a);

    return (uint16_t)((hi << 8) | rd(c, (uint16_t)(a + 1)));
}

static void wr16(cpu6803_t *c, uint16_t a, uint16_t v)
{
    wr(c, a, (uint8_t)(v >> 8));
    wr(c, (uint16_t)(a + 1), (uint8_t)v);
}

uint8_t cpu6803_peek(cpu6803_t *c, uint16_t a)
{
    if (a <= M68_REG_LAST) {
        switch (a) {
        case M68_REG_TCSR:   return c->tcsr;
        case M68_REG_CNT_H:  return (uint8_t)(c->counter >> 8);
        case M68_REG_CNT_L:  return (uint8_t)c->counter;
        case M68_REG_ICR_H:  return (uint8_t)(c->icr >> 8);
        default:             return reg_read(c, a);
        }
    }
    return rd(c, a);
}

/* ---------------------------------------------------------------------- */
/*  Fetching, the stack and the flags                                      */
/* ---------------------------------------------------------------------- */

static uint8_t fetch8(cpu6803_t *c)
{
    return rd(c, c->pc++);
}

static uint16_t fetch16(cpu6803_t *c)
{
    uint16_t hi = fetch8(c);

    return (uint16_t)((hi << 8) | fetch8(c));
}

static void push8(cpu6803_t *c, uint8_t v)
{
    wr(c, c->sp--, v);
}

static uint8_t pull8(cpu6803_t *c)
{
    return rd(c, ++c->sp);
}

static void push16(cpu6803_t *c, uint16_t v)    /* low byte first */
{
    push8(c, (uint8_t)v);
    push8(c, (uint8_t)(v >> 8));
}

static uint16_t pull16(cpu6803_t *c)
{
    uint16_t hi = pull8(c);

    return (uint16_t)((hi << 8) | pull8(c));
}

static void set_cc(cpu6803_t *c, uint8_t mask, bool on)
{
    if (on)
        c->cc |= mask;
    else
        c->cc &= (uint8_t)~mask;
}

static void flags_nz8(cpu6803_t *c, uint8_t r)
{
    set_cc(c, M68_CC_N, (r & 0x80) != 0);
    set_cc(c, M68_CC_Z, r == 0);
}

static void flags_nz16(cpu6803_t *c, uint16_t r)
{
    set_cc(c, M68_CC_N, (r & 0x8000) != 0);
    set_cc(c, M68_CC_Z, r == 0);
}

/* N and Z from the value, V cleared (loads, stores, logic) */
static uint8_t logic8(cpu6803_t *c, uint8_t r)
{
    flags_nz8(c, r);
    set_cc(c, M68_CC_V, false);
    return r;
}

static uint16_t logic16(cpu6803_t *c, uint16_t r)
{
    flags_nz16(c, r);
    set_cc(c, M68_CC_V, false);
    return r;
}

static uint8_t add8(cpu6803_t *c, uint8_t a, uint8_t m, int carry)
{
    unsigned r = (unsigned)a + m + (unsigned)carry;

    flags_nz8(c, (uint8_t)r);
    set_cc(c, M68_CC_H, ((a ^ m ^ r) & 0x10) != 0);
    set_cc(c, M68_CC_V, (~(a ^ m) & (a ^ r) & 0x80) != 0);
    set_cc(c, M68_CC_C, r > 0xFF);
    return (uint8_t)r;
}

static uint8_t sub8(cpu6803_t *c, uint8_t a, uint8_t m, int carry)
{
    unsigned r = (unsigned)a - m - (unsigned)carry;

    flags_nz8(c, (uint8_t)r);
    set_cc(c, M68_CC_V, ((a ^ m) & (a ^ r) & 0x80) != 0);
    set_cc(c, M68_CC_C, (unsigned)a < (unsigned)m + (unsigned)carry);
    return (uint8_t)r;
}

static uint16_t add16(cpu6803_t *c, uint16_t a, uint16_t m)
{
    uint32_t r = (uint32_t)a + m;

    flags_nz16(c, (uint16_t)r);
    set_cc(c, M68_CC_V, (~(a ^ m) & (a ^ r) & 0x8000) != 0);
    set_cc(c, M68_CC_C, r > 0xFFFF);
    return (uint16_t)r;
}

static uint16_t sub16(cpu6803_t *c, uint16_t a, uint16_t m)
{
    uint32_t r = (uint32_t)a - m;

    flags_nz16(c, (uint16_t)r);
    set_cc(c, M68_CC_V, ((a ^ m) & (a ^ r) & 0x8000) != 0);
    set_cc(c, M68_CC_C, a < m);
    return (uint16_t)r;
}

static uint16_t get_d(const cpu6803_t *c)
{
    return (uint16_t)((c->a << 8) | c->b);
}

static void set_d(cpu6803_t *c, uint16_t v)
{
    c->a = (uint8_t)(v >> 8);
    c->b = (uint8_t)v;
}

/* The one-operand instructions of the accumulators and memory (low nibble of
 * the opcode 0x40 to 0x7F). *store is cleared when the result is not written
 * back (TST).
 *
 * Author: Thomas Dzubin */
static uint8_t unary8(cpu6803_t *c, int lo, uint8_t v, bool *store)
{
    bool oc = (c->cc & M68_CC_C) != 0;
    bool nf;
    uint8_t r = v;

    *store = true;
    switch (lo) {
    case 0x0:                               /* NEG */
        r = (uint8_t)(0 - v);
        flags_nz8(c, r);
        set_cc(c, M68_CC_V, v == 0x80);
        set_cc(c, M68_CC_C, v != 0);
        return r;
    case 0x3:                               /* COM */
        r = (uint8_t)~v;
        flags_nz8(c, r);
        set_cc(c, M68_CC_V, false);
        set_cc(c, M68_CC_C, true);
        return r;
    case 0x4: r = (uint8_t)(v >> 1); set_cc(c, M68_CC_C, (v & 1) != 0); break;       /* LSR */
    case 0x6: r = (uint8_t)((v >> 1) | (oc ? 0x80 : 0));                             /* ROR */
              set_cc(c, M68_CC_C, (v & 1) != 0); break;
    case 0x7: r = (uint8_t)((v >> 1) | (v & 0x80));                                  /* ASR */
              set_cc(c, M68_CC_C, (v & 1) != 0); break;
    case 0x8: r = (uint8_t)(v << 1); set_cc(c, M68_CC_C, (v & 0x80) != 0); break;    /* ASL */
    case 0x9: r = (uint8_t)((v << 1) | (oc ? 1 : 0));                                /* ROL */
              set_cc(c, M68_CC_C, (v & 0x80) != 0); break;
    case 0xA:                               /* DEC */
        r = (uint8_t)(v - 1);
        flags_nz8(c, r);
        set_cc(c, M68_CC_V, v == 0x80);
        return r;
    case 0xC:                               /* INC */
        r = (uint8_t)(v + 1);
        flags_nz8(c, r);
        set_cc(c, M68_CC_V, v == 0x7F);
        return r;
    case 0xD:                               /* TST */
        flags_nz8(c, v);
        set_cc(c, M68_CC_V, false);
        set_cc(c, M68_CC_C, false);
        *store = false;
        return v;
    default:                                /* 0xF CLR */
        c->cc = (uint8_t)((c->cc & ~(M68_CC_N | M68_CC_V | M68_CC_C)) | M68_CC_Z);
        return 0;
    }
    /* the shifts and rotates: N, Z, and V = N xor C */
    flags_nz8(c, r);
    nf = (c->cc & M68_CC_N) != 0;
    set_cc(c, M68_CC_V, nf != ((c->cc & M68_CC_C) != 0));
    return r;
}

/* The two-operand accumulator instructions (low nibble of 0x80 to 0xFF, 0 to
 * B except STA): returns the new accumulator; *store is cleared for CMP/BIT. */
static uint8_t alu8(cpu6803_t *c, int lo, uint8_t a, uint8_t m, bool *store)
{
    int carry = (c->cc & M68_CC_C) != 0;

    *store = true;
    switch (lo) {
    case 0x0: return sub8(c, a, m, 0);
    case 0x1: *store = false; return sub8(c, a, m, 0);
    case 0x2: return sub8(c, a, m, carry);
    case 0x4: return logic8(c, (uint8_t)(a & m));
    case 0x5: *store = false; return logic8(c, (uint8_t)(a & m));
    case 0x6: return logic8(c, m);
    case 0x8: return logic8(c, (uint8_t)(a ^ m));
    case 0x9: return add8(c, a, m, carry);
    case 0xA: return logic8(c, (uint8_t)(a | m));
    default:  return add8(c, a, m, 0);          /* 0xB ADD */
    }
}

/* ---------------------------------------------------------------------- */
/*  Interrupts                                                             */
/* ---------------------------------------------------------------------- */

static int enter_interrupt(cpu6803_t *c, uint16_t vector)
{
    int n;

    if (c->wai) {                   /* WAI stacked everything already */
        n = M68_INT_WAI_CYCLES;
        c->wai = false;
    } else {
        push16(c, c->pc);
        push16(c, c->x);
        push8(c, c->a);
        push8(c, c->b);
        push8(c, (uint8_t)(c->cc | M68_CC_ONES));
        n = M68_INT_CYCLES;
    }
    c->cc |= M68_CC_I;
    c->pc = rd16(c, vector);
    tick(c, n);
    return n;
}

/* Take an interrupt if one is asked for; returns the cycles it took or 0.
 *
 * Author: Thomas Dzubin */
static int check_interrupts(cpu6803_t *c)
{
    if (c->nmi_pending) {
        c->nmi_pending = false;
        return enter_interrupt(c, M68_VEC_NMI);
    }
    if (c->cc & M68_CC_I)
        return 0;
    if (c->irq)
        return enter_interrupt(c, M68_VEC_IRQ1);
    if ((c->tcsr & (M68_TCSR_EICI | M68_TCSR_ICF)) == (M68_TCSR_EICI | M68_TCSR_ICF))
        return enter_interrupt(c, M68_VEC_ICF);
    if ((c->tcsr & (M68_TCSR_EOCI | M68_TCSR_OCF)) == (M68_TCSR_EOCI | M68_TCSR_OCF))
        return enter_interrupt(c, M68_VEC_OCF);
    if ((c->tcsr & (M68_TCSR_ETOI | M68_TCSR_TOF)) == (M68_TCSR_ETOI | M68_TCSR_TOF))
        return enter_interrupt(c, M68_VEC_TOF);
    if (((c->trcsr & M68_TRCSR_RIE) && (c->trcsr & (M68_TRCSR_RDRF | M68_TRCSR_ORFE))) ||
        (c->trcsr & (M68_TRCSR_TIE | M68_TRCSR_TDRE)) == (M68_TRCSR_TIE | M68_TRCSR_TDRE))
        return enter_interrupt(c, M68_VEC_SCI);
    return 0;
}

void cpu6803_nmi(cpu6803_t *c, bool asserted)
{
    if (asserted)
        c->nmi_pending = true;
}

/* ---------------------------------------------------------------------- */
/*  Reset                                                                  */
/* ---------------------------------------------------------------------- */

/* Author: Thomas Dzubin */
void cpu6803_reset(cpu6803_t *c)
{
    memset(c->ddr, 0, sizeof c->ddr);
    c->a = c->b = 0;
    c->x = c->sp = 0;
    c->cc = M68_CC_I;
    c->wai = false;
    c->trapped = false;
    c->defer_int = false;
    c->nmi_pending = false;
    c->tcsr = 0;
    c->pending = 0;
    c->counter = 0;
    c->ocr = M68_OCR_RESET;
    c->icr = 0;
    c->counter_low_held = false;
    c->rmcr = 0;
    c->trcsr = M68_TRCSR_TDRE;
    c->rdr = 0;
    c->p3csr = 0;
    c->rcr = M68_RCR_RAME;
    c->ram_enabled = true;
    c->tin = false;
    c->cycles = 0;
    c->pc = rd16(c, M68_VEC_RESET);
}

/* ---------------------------------------------------------------------- */
/*  One instruction                                                        */
/* ---------------------------------------------------------------------- */

/* Which condition the branch opcodes 0x20 to 0x2F test (low nibble).
 *
 * Author: Thomas Dzubin */
static bool branch_taken(const cpu6803_t *c, int lo)
{
    bool cf = (c->cc & M68_CC_C) != 0;
    bool zf = (c->cc & M68_CC_Z) != 0;
    bool nf = (c->cc & M68_CC_N) != 0;
    bool vf = (c->cc & M68_CC_V) != 0;

    switch (lo) {
    case 0x0: return true;
    case 0x1: return false;
    case 0x2: return !(cf || zf);
    case 0x3: return cf || zf;
    case 0x4: return !cf;
    case 0x5: return cf;
    case 0x6: return !zf;
    case 0x7: return zf;
    case 0x8: return !vf;
    case 0x9: return vf;
    case 0xA: return !nf;
    case 0xB: return nf;
    case 0xC: return nf == vf;
    case 0xD: return nf != vf;
    case 0xE: return !(zf || nf != vf);
    default:  return zf || nf != vf;
    }
}

static void do_daa(cpu6803_t *c)
{
    unsigned msn = c->a & 0xF0, lsn = c->a & 0x0F, cf = 0, t;

    if (lsn > 0x09 || (c->cc & M68_CC_H))
        cf |= 0x06;
    if (msn > 0x80 && lsn > 0x09)
        cf |= 0x60;
    if (msn > 0x90 || (c->cc & M68_CC_C))
        cf |= 0x60;
    t = cf + c->a;
    c->a = (uint8_t)t;
    flags_nz8(c, c->a);
    set_cc(c, M68_CC_V, false);
    if (t > 0xFF)
        set_cc(c, M68_CC_C, true);
}

/* The inherent instructions 0x00 to 0x1F, 0x30 to 0x3F and 0x40 to 0x5F that
 * are not the one-operand family; returns false for an illegal opcode.
 *
 * Author: Thomas Dzubin */
static bool inherent(cpu6803_t *c, uint8_t op)
{
    uint16_t d;
    uint32_t r;

    switch (op) {
    case 0x01: break;                                           /* NOP */
    case 0x04:                                                  /* LSRD */
        d = get_d(c);
        set_cc(c, M68_CC_C, (d & 1) != 0);
        d >>= 1;
        set_d(c, d);
        flags_nz16(c, d);
        set_cc(c, M68_CC_N, false);
        set_cc(c, M68_CC_V, (c->cc & M68_CC_C) != 0);
        break;
    case 0x05:                                                  /* ASLD */
        d = get_d(c);
        set_cc(c, M68_CC_C, (d & 0x8000) != 0);
        d = (uint16_t)(d << 1);
        set_d(c, d);
        flags_nz16(c, d);
        set_cc(c, M68_CC_V, ((c->cc & M68_CC_N) != 0) != ((c->cc & M68_CC_C) != 0));
        break;
    case 0x06:                                                  /* TAP */
        c->cc = (uint8_t)(c->a & M68_CC_MASK);
        c->defer_int = true;
        break;
    case 0x07: c->a = (uint8_t)(c->cc | M68_CC_ONES); break;    /* TPA */
    case 0x08: c->x++; set_cc(c, M68_CC_Z, c->x == 0); break;   /* INX */
    case 0x09: c->x--; set_cc(c, M68_CC_Z, c->x == 0); break;   /* DEX */
    case 0x0A: c->cc &= (uint8_t)~M68_CC_V; break;
    case 0x0B: c->cc |= M68_CC_V; break;
    case 0x0C: c->cc &= (uint8_t)~M68_CC_C; break;
    case 0x0D: c->cc |= M68_CC_C; break;
    case 0x0E:                                                  /* CLI */
        if (c->cc & M68_CC_I)
            c->defer_int = true;
        c->cc &= (uint8_t)~M68_CC_I;
        break;
    case 0x0F: c->cc |= M68_CC_I; break;
    case 0x10: c->a = sub8(c, c->a, c->b, 0); break;            /* SBA */
    case 0x11: (void)sub8(c, c->a, c->b, 0); break;             /* CBA */
    case 0x16: c->b = c->a; logic8(c, c->b); break;            /* TAB */
    case 0x17: c->a = c->b; logic8(c, c->a); break;            /* TBA */
    case 0x19: do_daa(c); break;
    case 0x1B: c->a = add8(c, c->a, c->b, 0); break;            /* ABA */
    case 0x30: c->x = (uint16_t)(c->sp + 1); break;             /* TSX */
    case 0x31: c->sp++; break;                                  /* INS */
    case 0x32: c->a = pull8(c); break;                          /* PULA */
    case 0x33: c->b = pull8(c); break;                          /* PULB */
    case 0x34: c->sp--; break;                                  /* DES */
    case 0x35: c->sp = (uint16_t)(c->x - 1); break;             /* TXS */
    case 0x36: push8(c, c->a); break;                           /* PSHA */
    case 0x37: push8(c, c->b); break;                           /* PSHB */
    case 0x38: c->x = pull16(c); break;                         /* PULX */
    case 0x39: c->pc = pull16(c); break;                        /* RTS */
    case 0x3A: c->x = (uint16_t)(c->x + c->b); break;           /* ABX */
    case 0x3B:                                                  /* RTI */
        c->cc = (uint8_t)(pull8(c) & M68_CC_MASK);
        c->b = pull8(c);
        c->a = pull8(c);
        c->x = pull16(c);
        c->pc = pull16(c);
        break;
    case 0x3C: push16(c, c->x); break;                          /* PSHX */
    case 0x3D:                                                  /* MUL */
        r = (uint32_t)c->a * c->b;
        set_d(c, (uint16_t)r);
        set_cc(c, M68_CC_C, (r & 0x80) != 0);
        break;
    case 0x3E:                                                  /* WAI */
        push16(c, c->pc);
        push16(c, c->x);
        push8(c, c->a);
        push8(c, c->b);
        push8(c, (uint8_t)(c->cc | M68_CC_ONES));
        c->wai = true;
        break;
    case 0x3F:                                                  /* SWI */
        push16(c, c->pc);
        push16(c, c->x);
        push8(c, c->a);
        push8(c, c->b);
        push8(c, (uint8_t)(c->cc | M68_CC_ONES));
        c->cc |= M68_CC_I;
        c->pc = rd16(c, M68_VEC_SWI);
        break;
    default:
        return false;
    }
    return true;
}

/* The effective address of the operand of a memory-reference opcode, by the
 * high nibble's mode: 0x6 and 0xA and 0xE indexed, 0x7 and 0xB and 0xF
 * extended, 0x9 and 0xD direct, 0x8 and 0xC immediate (a 16-bit operand when
 * wide). */
static uint16_t operand_address(cpu6803_t *c, int mode, bool wide)
{
    uint16_t a;

    switch (mode) {
    case 0:                                 /* immediate */
        a = c->pc;
        c->pc = (uint16_t)(c->pc + (wide ? 2 : 1));
        return a;
    case 1: return fetch8(c);               /* direct */
    case 2: return (uint16_t)(c->x + fetch8(c));
    default: return fetch16(c);             /* extended */
    }
}

/* Opcodes 0x80 to 0xFF. Returns false for an illegal one.
 *
 * Author: Thomas Dzubin */
static bool memory_op(cpu6803_t *c, uint8_t op)
{
    int lo = op & 0x0F;
    int mode = (op >> 4) & 3;               /* 0 imm, 1 direct, 2 indexed, 3 ext */
    bool bacc = (op & 0x40) != 0;           /* 0xC0 to 0xFF: B, LDD, ADDD, LDX */
    bool store;
    uint16_t ea, w;
    uint8_t *acc = bacc ? &c->b : &c->a;

    switch (lo) {
    case 0x7:                               /* STA (not immediate) */
        if (mode == 0)
            return false;
        ea = operand_address(c, mode, false);
        wr(c, ea, logic8(c, *acc));
        return true;
    case 0x3:                               /* SUBD (8x-Bx) or ADDD (Cx-Fx) */
        ea = operand_address(c, mode, true);
        w = rd16(c, ea);
        set_d(c, bacc ? add16(c, get_d(c), w) : sub16(c, get_d(c), w));
        return true;
    case 0xC:
        if (!bacc) {                        /* CPX */
            ea = operand_address(c, mode, true);
            (void)sub16(c, c->x, rd16(c, ea));
        } else {                            /* LDD */
            ea = operand_address(c, mode, true);
            set_d(c, logic16(c, rd16(c, ea)));
        }
        return true;
    case 0xD:
        if (!bacc) {                        /* BSR (immediate slot), JSR */
            if (mode == 0) {
                int8_t off = (int8_t)fetch8(c);

                push16(c, c->pc);
                c->pc = (uint16_t)(c->pc + off);
                return true;
            }
            ea = operand_address(c, mode, false);
            push16(c, c->pc);
            c->pc = ea;
            return true;
        }
        if (mode == 0)                      /* 0xCD */
            return false;
        ea = operand_address(c, mode, false);   /* STD */
        w = logic16(c, get_d(c));
        wr16(c, ea, w);
        return true;
    case 0xE:                               /* LDS (8x-Bx), LDX (Cx-Fx) */
        ea = operand_address(c, mode, true);
        w = logic16(c, rd16(c, ea));
        if (bacc)
            c->x = w;
        else
            c->sp = w;
        return true;
    case 0xF:                               /* STS (9x-Bx), STX (Dx-Fx) */
        if (mode == 0)
            return false;
        ea = operand_address(c, mode, false);
        w = logic16(c, bacc ? c->x : c->sp);
        wr16(c, ea, w);
        return true;
    default:                                /* SUB CMP SBC AND BIT LDA EOR ADC ORA ADD */
        ea = operand_address(c, mode, false);
        {
            uint8_t r = alu8(c, lo, *acc, rd(c, ea), &store);

            if (store)
                *acc = r;
        }
        return true;
    }
}

/* One step.
 *
 * Author: Thomas Dzubin */
int cpu6803_step(cpu6803_t *c)
{
    uint8_t op;
    int n;
    bool ok = true;
    bool store;

    if (c->defer_int)                       /* the instruction after CLI or TAP runs first */
        c->defer_int = false;
    else {
        n = check_interrupts(c);
        if (n)
            return n;
    }
    if (c->wai) {                           /* idle one cycle at a time */
        tick(c, 1);
        return 1;
    }

    c->trapped = false;
    op = fetch8(c);
    n = cycle_table[op];

    if (op >= 0x80) {
        ok = memory_op(c, op);
    } else if (op >= 0x60) {                /* one-operand memory: indexed, extended */
        int lo = op & 0x0F;

        if (lo == 0xE) {                    /* JMP */
            c->pc = (op & 0x10) ? fetch16(c) : (uint16_t)(c->x + fetch8(c));
        } else if (lo == 0x1 || lo == 0x2 || lo == 0x5 || lo == 0xB) {
            ok = false;
        } else {
            uint16_t ea = (op & 0x10) ? fetch16(c) : (uint16_t)(c->x + fetch8(c));
            /* every one reads the operand first, CLR too (it matters at the on-chip
             * registers, where a read has side effects) */
            uint8_t v = unary8(c, lo, rd(c, ea), &store);

            if (store)
                wr(c, ea, v);
        }
    } else if (op >= 0x40) {                /* one-operand accumulator */
        int lo = op & 0x0F;

        if (lo == 0x1 || lo == 0x2 || lo == 0x5 || lo == 0xB || lo == 0xE) {
            ok = false;
        } else {
            uint8_t *acc = (op & 0x10) ? &c->b : &c->a;
            uint8_t v = unary8(c, lo, *acc, &store);

            if (store)
                *acc = v;
        }
    } else if (op >= 0x20 && op < 0x30) {   /* branches */
        int8_t off = (int8_t)fetch8(c);

        if (branch_taken(c, op & 0x0F))
            c->pc = (uint16_t)(c->pc + off);
    } else {
        ok = inherent(c, op);
    }

    if (!ok) {                              /* illegal opcode: the trap */
        c->trapped = true;
        push16(c, c->pc);
        push16(c, c->x);
        push8(c, c->a);
        push8(c, c->b);
        push8(c, (uint8_t)(c->cc | M68_CC_ONES));
        c->cc |= M68_CC_I;
        c->pc = rd16(c, M68_VEC_TRAP);
        n = M68_INT_CYCLES;
    }
    tick(c, n);
    return n;
}
