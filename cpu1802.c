/*
 * cpu1802.c - the CDP1802 instruction set and its machine-cycle sequencing.
 * See cpu1802.h for the interface.
 *
 * Sequencing, as in the RCA data sheet's state diagram:
 *
 *   - After a reset ends there is one initialization cycle (9 clocks) that
 *     clears X, P and R0. The cycle after it can be S0, S1 or S2, never S3.
 *   - S0 is always followed by S1. A long branch, long skip or NOP has a
 *     second S1 that nothing can interrupt.
 *   - After the last S1 of an instruction, after an S2 and after an S3 the
 *     requests are sampled: DMA-IN first, then DMA-OUT, then INTERRUPT (only
 *     with IE set), else the next instruction is fetched.
 *   - IDL keeps the CPU in S1 until a DMA or interrupt request comes. A DMA
 *     cycle does not end the idling; an interrupt does.
 *   - In load mode the CPU idles and has no clock pulses; only a DMA request
 *     (or interrupt) makes it run a cycle.
 *
 * An instruction does all its work in its first S1 cycle. That is exact as
 * far as any outside observer can tell, since the bus is only sampled by the
 * callbacks at that point and the second S1 of a long instruction can not be
 * interrupted.
 *
 * Author: Thomas Dzubin
 */
#include "cpu1802.h"
#include "elf_const.h"

/* What execute() tells the sequencer to do next. */
#define EXEC_DONE   0
#define EXEC_LONG   1             /* a second S1 follows */
#define EXEC_IDLE   2             /* IDL: stay in S1 until a request */

static inline uint8_t rd(cpu1802_t *c, uint16_t addr)
{
    return c->mem_read(c->ctx, addr);
}

static inline void wr(cpu1802_t *c, uint16_t addr, uint8_t data)
{
    c->mem_write(c->ctx, addr, data);
}

/* The byte at R(P), then R(P) steps on (data immediate, branch operands). */
static inline uint8_t fetch_next(cpu1802_t *c)
{
    return rd(c, c->r[c->p]++);
}

/* D = a + b + carry, DF = carry out. */
static inline void add_to_d(cpu1802_t *c, uint8_t a, uint8_t b, unsigned carry)
{
    unsigned s = (unsigned)a + b + carry;

    c->d = (uint8_t)s;
    c->df = (s >> 8) != 0;
}

/* The arithmetic and logic group F0 to FF, and the carry versions at 74, 75,
 * 77, 7C, 7D, 7F. "which" is the low nibble with the same meaning in both
 * groups: 4 add, 5 subtract D from M, 7 subtract M from D. */
static void alu(cpu1802_t *c, unsigned which, uint8_t m, unsigned carry_in)
{
    switch (which) {
    case 0x1: c->d |= m;  break;                          /* OR  */
    case 0x2: c->d &= m;  break;                          /* AND */
    case 0x3: c->d ^= m;  break;                          /* XOR */
    case 0x4: add_to_d(c, c->d, m, carry_in); break;      /* ADD, ADC */
    case 0x5: add_to_d(c, m, (uint8_t)~c->d, carry_in); break;   /* SD, SDB */
    case 0x7: add_to_d(c, c->d, (uint8_t)~m, carry_in); break;   /* SM, SMB */
    default: break;
    }
}

/* The condition of a short branch (3x) or short skip, from the low nibble. The
 * flag inputs are true when the pin is LOW (data sheet: "B1: branch if EF1 = 1,
 * EF1 = VSS"), so B1 to B4 branch on a low pin and BN1 to BN4 on a high one.
 *
 * Author: Thomas Dzubin */
static bool short_condition(const cpu1802_t *c, unsigned n)
{
    switch (n) {
    case 0x0: return true;                       /* BR  */
    case 0x1: return c->q;                       /* BQ  */
    case 0x2: return c->d == 0;                  /* BZ  */
    case 0x3: return c->df;                      /* BDF */
    case 0x4: return (c->ef & CPU_EF1) == 0;     /* B1  */
    case 0x5: return (c->ef & CPU_EF2) == 0;     /* B2  */
    case 0x6: return (c->ef & CPU_EF3) == 0;     /* B3  */
    case 0x7: return (c->ef & CPU_EF4) == 0;     /* B4  */
    case 0x8: return false;                      /* SKP, the no-branch */
    case 0x9: return !c->q;                      /* BNQ */
    case 0xA: return c->d != 0;                  /* BNZ */
    case 0xB: return !c->df;                     /* BNF */
    case 0xC: return (c->ef & CPU_EF1) != 0;     /* BN1 */
    case 0xD: return (c->ef & CPU_EF2) != 0;     /* BN2 */
    case 0xE: return (c->ef & CPU_EF3) != 0;     /* BN3 */
    default:  return (c->ef & CPU_EF4) != 0;     /* BN4 */
    }
}

/* Opcode Cx: long branches (take a 16-bit address), long skips (skip two
 * bytes) and NOP. Branch taken: R(P) = the two bytes at R(P). Not taken, or a
 * skip that skips: R(P) steps over them.
 *
 * Author: Thomas Dzubin */
static void long_group(cpu1802_t *c, unsigned n)
{
    bool is_branch = ((CPU_LONG_BRANCH_MASK >> n) & 1) != 0;
    bool take;                      /* the branch is taken, or the skip skips */

    switch (n) {
    case 0x0: take = true;          break;       /* LBR  */
    case 0x1: take = c->q;          break;       /* LBQ  */
    case 0x2: take = c->d == 0;     break;       /* LBZ  */
    case 0x3: take = c->df;         break;       /* LBDF */
    case 0x4:                       return;      /* NOP  */
    case 0x5: take = !c->q;         break;       /* LSNQ */
    case 0x6: take = c->d != 0;     break;       /* LSNZ */
    case 0x7: take = !c->df;        break;       /* LSNF */
    case 0x8: take = true;          break;       /* LSKP */
    case 0x9: take = !c->q;         break;       /* LBNQ */
    case 0xA: take = c->d != 0;     break;       /* LBNZ */
    case 0xB: take = !c->df;        break;       /* LBNF */
    case 0xC: take = c->ie;         break;       /* LSIE */
    case 0xD: take = c->q;          break;       /* LSQ  */
    case 0xE: take = c->d == 0;     break;       /* LSZ  */
    default:  take = c->df;         break;       /* LSDF */
    }

    if (is_branch && take) {
        uint8_t hi = rd(c, c->r[c->p]);
        uint8_t lo = rd(c, (uint16_t)(c->r[c->p] + 1));

        c->r[c->p] = (uint16_t)((hi << 8) | lo);
    } else if (is_branch || take) {
        c->r[c->p] += 2;            /* a branch not taken, or a skip taken */
    }
}

/* Opcode 6x: IRX, OUT 1 to 7, the unused 68, INP 1 to 7. */
static void io_group(cpu1802_t *c, unsigned n)
{
    if (n == 0) {                                /* IRX */
        c->r[c->x]++;
    } else if (n < 8) {                          /* OUT n */
        uint8_t data = rd(c, c->r[c->x]);

        c->r[c->x]++;
        c->io_out(c->ctx, (uint8_t)n, data);
    } else {                                     /* INP n-8 (68 is port 0) */
        uint8_t data = c->io_in(c->ctx, (uint8_t)(n - 8));

        wr(c, c->r[c->x], data);
        c->d = data;
    }
}

/* Opcode 7x: the return and save group, carry arithmetic, the shifts through
 * carry, REQ and SEQ.
 *
 * Author: Thomas Dzubin */
static void misc_group(cpu1802_t *c, unsigned n)
{
    uint8_t v;

    switch (n) {
    case 0x0:                                    /* RET */
    case 0x1:                                    /* DIS */
        v = rd(c, c->r[c->x]);
        c->r[c->x]++;
        c->x = v >> 4;
        c->p = v & 0x0F;
        c->ie = (n == 0x0);
        break;
    case 0x2:                                    /* LDXA */
        c->d = rd(c, c->r[c->x]);
        c->r[c->x]++;
        break;
    case 0x3:                                    /* STXD */
        wr(c, c->r[c->x], c->d);
        c->r[c->x]--;
        break;
    case 0x4: alu(c, 0x4, rd(c, c->r[c->x]), c->df); break;       /* ADC  */
    case 0x5: alu(c, 0x5, rd(c, c->r[c->x]), c->df); break;       /* SDB  */
    case 0x6:                                    /* SHRC (RSHR) */
        v = c->d;
        c->d = (uint8_t)((c->d >> 1) | (c->df ? 0x80 : 0));
        c->df = (v & 1) != 0;
        break;
    case 0x7: alu(c, 0x7, rd(c, c->r[c->x]), c->df); break;       /* SMB  */
    case 0x8:                                    /* SAV */
        wr(c, c->r[c->x], c->t);
        break;
    case 0x9:                                    /* MARK */
        c->t = (uint8_t)((c->x << 4) | c->p);
        wr(c, c->r[2], c->t);
        c->x = c->p;
        c->r[2]--;
        break;
    case 0xA: c->q = false; break;               /* REQ */
    case 0xB: c->q = true;  break;               /* SEQ */
    case 0xC: alu(c, 0x4, fetch_next(c), c->df); break;           /* ADCI */
    case 0xD: alu(c, 0x5, fetch_next(c), c->df); break;           /* SDBI */
    case 0xE:                                    /* SHLC (RSHL) */
        v = c->d;
        c->d = (uint8_t)((c->d << 1) | (c->df ? 1 : 0));
        c->df = (v & 0x80) != 0;
        break;
    default:  alu(c, 0x7, fetch_next(c), c->df); break;           /* SMBI */
    }
}

/* Opcode Fx: load, logic, add, subtract, shifts and the immediate forms.
 *
 * Author: Thomas Dzubin */
static void f_group(cpu1802_t *c, unsigned n)
{
    uint8_t m;

    if (n == 0x6) {                              /* SHR */
        c->df = (c->d & 1) != 0;
        c->d >>= 1;
        return;
    }
    if (n == 0xE) {                              /* SHL */
        c->df = (c->d & 0x80) != 0;
        c->d = (uint8_t)(c->d << 1);
        return;
    }

    m = (n & 8) ? fetch_next(c) : rd(c, c->r[c->x]);
    switch (n & 7) {
    case 0x0: c->d = m;  break;                              /* LDX, LDI */
    case 0x1: alu(c, 0x1, m, 0); break;                      /* OR,  ORI */
    case 0x2: alu(c, 0x2, m, 0); break;                      /* AND, ANI */
    case 0x3: alu(c, 0x3, m, 0); break;                      /* XOR, XRI */
    case 0x4: alu(c, 0x4, m, 0); break;                      /* ADD, ADI */
    case 0x5: alu(c, 0x5, m, 1); break;                      /* SD,  SDI */
    default:  alu(c, 0x7, m, 1); break;                      /* SM,  SMI */
    }
}

/* Carry out the instruction in I and N.
 *
 * Author: Thomas Dzubin */
static int execute(cpu1802_t *c)
{
    unsigned n = c->n;

    switch (c->i) {
    case 0x0:                                    /* IDL, LDN */
        if (n == 0)
            return EXEC_IDLE;
        c->d = rd(c, c->r[n]);
        break;
    case 0x1: c->r[n]++; break;                  /* INC */
    case 0x2: c->r[n]--; break;                  /* DEC */
    case 0x3:                                    /* short branches */
        if (short_condition(c, n))
            c->r[c->p] = (uint16_t)((c->r[c->p] & 0xFF00) | rd(c, c->r[c->p]));
        else
            c->r[c->p]++;
        break;
    case 0x4:                                    /* LDA */
        c->d = rd(c, c->r[n]);
        c->r[n]++;
        break;
    case 0x5: wr(c, c->r[n], c->d); break;       /* STR */
    case 0x6: io_group(c, n); break;
    case 0x7: misc_group(c, n); break;
    case 0x8: c->d = (uint8_t)c->r[n]; break;                          /* GLO */
    case 0x9: c->d = (uint8_t)(c->r[n] >> 8); break;                   /* GHI */
    case 0xA: c->r[n] = (uint16_t)((c->r[n] & 0xFF00) | c->d); break;  /* PLO */
    case 0xB: c->r[n] = (uint16_t)((c->r[n] & 0x00FF) | (c->d << 8)); break; /* PHI */
    case 0xC:
        long_group(c, n);
        return EXEC_LONG;
    case 0xD: c->p = (uint8_t)n; break;          /* SEP */
    case 0xE: c->x = (uint8_t)n; break;          /* SEX */
    default:  f_group(c, n); break;
    }
    return EXEC_DONE;
}

/* The pins say which kind of cycle comes next at a decision point. */
static int choose(const cpu1802_t *c, bool interrupt_allowed, bool idling)
{
    if (c->dma_in_req || c->dma_out_req)
        return CPU_CYCLE_S2;
    if (interrupt_allowed && c->ie && c->int_req)
        return CPU_CYCLE_S3;
    return idling ? CPU_CYCLE_S1 : CPU_CYCLE_S0;
}

/* S2: one DMA byte to or from the memory R(0) points at. DMA-IN wins. */
static void dma_cycle(cpu1802_t *c)
{
    if (c->dma_in_req)
        wr(c, c->r[0], c->dma_in(c->ctx));
    else
        c->dma_out(c->ctx, rd(c, c->r[0]));
    c->r[0]++;
}

/* S3: save X and P in T, run the interrupt routine from R(1) with R(2) as the
 * stack, interrupts off. */
static void interrupt_cycle(cpu1802_t *c)
{
    c->t = (uint8_t)((c->x << 4) | c->p);
    c->x = 2;
    c->p = 1;
    c->ie = false;
}

void cpu1802_power_on(cpu1802_t *c)
{
    int k;

    for (k = 0; k < 16; k++)
        c->r[k] = 0;
    c->d = c->t = c->x = c->p = c->i = c->n = 0;
    c->df = c->q = false;
    c->ie = true;
    c->mode = CPU_MODE_RESET;
    c->ef = 0x0F;
    c->int_req = c->dma_in_req = c->dma_out_req = false;
    c->cycles = 0;
    c->clocks = 0;
    c->phase = CPU_PHASE_RESET;
}

/* Author: Thomas Dzubin */
int cpu1802_cycle(cpu1802_t *c)
{
    int kind;

    if (c->mode == CPU_MODE_RESET) {             /* I, N, Q cleared, IE set */
        c->i = c->n = 0;
        c->q = false;
        c->ie = true;
        c->phase = CPU_PHASE_RESET;
        return CPU_CYCLE_NONE;
    }
    if (c->mode == CPU_MODE_PAUSE)
        return CPU_CYCLE_NONE;

    if (c->phase == CPU_PHASE_RESET) {           /* the initialization cycle */
        c->x = c->p = 0;
        c->r[0] = 0;
        c->phase = CPU_PHASE_FIRST;
        c->clocks = CPU_INIT_CLOCKS;
        c->cycles++;
        return CPU_CYCLE_INIT;
    }

    if (c->mode == CPU_MODE_LOAD) {              /* held idle, no clock pulses */
        if (c->dma_in_req || c->dma_out_req)
            kind = CPU_CYCLE_S2;
        else if (c->ie && c->int_req)
            kind = CPU_CYCLE_S3;
        else
            return CPU_CYCLE_NONE;
        c->phase = CPU_PHASE_LOAD;
    } else {
        switch (c->phase) {
        case CPU_PHASE_EXEC:
            kind = CPU_CYCLE_S1;
            break;
        case CPU_PHASE_EXEC2:
            kind = CPU_CYCLE_S1;
            break;
        case CPU_PHASE_FIRST:                    /* never S3 right after reset */
            kind = choose(c, false, false);
            break;
        case CPU_PHASE_IDLE:
            kind = choose(c, true, true);
            break;
        default:
            kind = choose(c, true, false);
            break;
        }
    }

    c->clocks = CPU_CLOCKS_PER_CYCLE;
    c->cycles++;

    switch (kind) {
    case CPU_CYCLE_S0: {
        uint8_t op = rd(c, c->r[c->p]++);

        c->i = op >> 4;
        c->n = op & 0x0F;
        c->phase = CPU_PHASE_EXEC;
        break;
    }
    case CPU_CYCLE_S1:
        if (c->phase == CPU_PHASE_EXEC) {
            switch (execute(c)) {
            case EXEC_LONG: c->phase = CPU_PHASE_EXEC2; break;
            case EXEC_IDLE: c->phase = CPU_PHASE_IDLE;  break;
            default:        c->phase = CPU_PHASE_NEXT;  break;
            }
        } else if (c->phase == CPU_PHASE_EXEC2) {
            c->phase = CPU_PHASE_NEXT;
        }                                        /* else an idle S1 */
        break;
    case CPU_CYCLE_S2:
        dma_cycle(c);
        if (c->phase != CPU_PHASE_IDLE && c->phase != CPU_PHASE_LOAD)
            c->phase = CPU_PHASE_NEXT;
        break;
    default:                                     /* S3 */
        interrupt_cycle(c);
        c->phase = CPU_PHASE_NEXT;
        break;
    }
    return kind;
}
