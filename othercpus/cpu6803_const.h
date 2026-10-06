/*
 * cpu6803_const.h - constants of the Motorola MC6803 (cpu6803.c): condition
 * code bits, interrupt vectors, the on-chip register addresses and bits, and
 * the clock cycles every opcode takes.
 *
 * Author: Thomas Dzubin
 */
#ifndef CPU6803_CONST_H
#define CPU6803_CONST_H

/* Condition code register. Bits 7 and 6 always read as 1. */
#define M68_CC_C        0x01    /* carry / borrow                            */
#define M68_CC_V        0x02    /* overflow                                  */
#define M68_CC_Z        0x04    /* zero                                      */
#define M68_CC_N        0x08    /* negative                                  */
#define M68_CC_I        0x10    /* interrupt mask                            */
#define M68_CC_H        0x20    /* half carry                                */
#define M68_CC_ONES     0xC0    /* the two bits that are always set          */
#define M68_CC_MASK     0x3F    /* the six that are stored                   */

/* Interrupt vectors. */
#define M68_VEC_SCI     0xFFF0
#define M68_VEC_TOF     0xFFF2
#define M68_VEC_OCF     0xFFF4
#define M68_VEC_ICF     0xFFF6
#define M68_VEC_IRQ1    0xFFF8
#define M68_VEC_SWI     0xFFFA
#define M68_VEC_NMI     0xFFFC
#define M68_VEC_RESET   0xFFFE
#define M68_VEC_TRAP    0xFFEE      /* illegal opcode */

/* Cycles an interrupt takes: the full entry, or just the vector fetch when
 * WAI has already stacked the registers. */
#define M68_INT_CYCLES      12
#define M68_INT_WAI_CYCLES  4

/* On-chip memory map: registers at 0x00 to 0x14, 128 bytes of RAM from 0x80. */
#define M68_REG_LAST    0x14
#define M68_RAM_BASE    0x80
#define M68_RAM_SIZE    0x80

#define M68_REG_DDR1    0x00
#define M68_REG_DDR2    0x01
#define M68_REG_P1      0x02
#define M68_REG_P2      0x03
#define M68_REG_DDR3    0x04
#define M68_REG_DDR4    0x05
#define M68_REG_P3      0x06
#define M68_REG_P4      0x07
#define M68_REG_TCSR    0x08
#define M68_REG_CNT_H   0x09
#define M68_REG_CNT_L   0x0A
#define M68_REG_OCR_H   0x0B
#define M68_REG_OCR_L   0x0C
#define M68_REG_ICR_H   0x0D
#define M68_REG_ICR_L   0x0E
#define M68_REG_P3CSR   0x0F
#define M68_REG_RMCR    0x10
#define M68_REG_TRCSR   0x11
#define M68_REG_RDR     0x12
#define M68_REG_TDR     0x13
#define M68_REG_RCR     0x14

/* Timer control and status register. */
#define M68_TCSR_OLVL   0x01    /* output level after a compare match        */
#define M68_TCSR_IEDG   0x02    /* input capture edge: 1 = rising            */
#define M68_TCSR_ETOI   0x04    /* enable the overflow interrupt             */
#define M68_TCSR_EOCI   0x08    /* enable the output compare interrupt       */
#define M68_TCSR_EICI   0x10    /* enable the input capture interrupt        */
#define M68_TCSR_TOF    0x20    /* timer overflow flag                       */
#define M68_TCSR_OCF    0x40    /* output compare flag                       */
#define M68_TCSR_ICF    0x80    /* input capture flag                        */
#define M68_TCSR_WRITE  0x1F    /* the bits a write can change               */
#define M68_TCSR_FLAGS  0xE0    /* the three flags, which a write cannot set */

/* Serial interface: transmit/receive control and status register. */
#define M68_TRCSR_RDRF  0x80
#define M68_TRCSR_ORFE  0x40
#define M68_TRCSR_TDRE  0x20
#define M68_TRCSR_RIE   0x10
#define M68_TRCSR_TIE   0x04
#define M68_TRCSR_WRITE 0x1F

/* RAM control register. */
#define M68_RCR_RAME    0x40    /* the on-chip RAM is enabled                */
#define M68_RCR_STORED  0xC0    /* the two bits that are kept (STBY PWR, RAME) */
#define M68_RCR_ONES    0x3F    /* the others always read as 1               */

/* Port 2: the output compare drives P21; the serial interface uses P23, P24. */
#define M68_P2_OC_BIT   0x02
#define M68_P2_MASK     0x1F

/* What an address that nothing answers, and a register nobody can read, gives. */
#define M68_OPEN_BUS    0xFF

/* Output compare register after reset. */
#define M68_OCR_RESET   0xFFFF

/* Writing the high byte of the counter sets it to this. */
#define M68_COUNTER_PRESET  0xFFF8
#define M68_COUNTER_FULL    0xFFFF  /* TOF is set when the counter reaches it */

/* Placeholder cycle count for the opcodes that trap. */
#define XX 4

/* Machine cycles (E clock) of each opcode, indexed by the opcode. */
#define CPU6803_CYCLES { \
    /* 0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F */ \
    /*0*/ XX, 2,XX,XX, 3, 3, 2, 2, 3, 3, 2, 2, 2, 2, 2, 2, \
    /*1*/  2, 2,XX,XX,XX,XX, 2, 2,XX, 2,XX, 2,XX,XX,XX,XX, \
    /*2*/  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, \
    /*3*/  3, 3, 4, 4, 3, 3, 3, 3, 5, 5, 3,10, 4,10, 9,12, \
    /*4*/  2,XX,XX, 2, 2,XX, 2, 2, 2, 2, 2,XX, 2, 2,XX, 2, \
    /*5*/  2,XX,XX, 2, 2,XX, 2, 2, 2, 2, 2,XX, 2, 2,XX, 2, \
    /*6*/  6,XX,XX, 6, 6,XX, 6, 6, 6, 6, 6,XX, 6, 6, 3, 6, \
    /*7*/  6,XX,XX, 6, 6,XX, 6, 6, 6, 6, 6,XX, 6, 6, 3, 6, \
    /*8*/  2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 2, 4, 6, 3, 3, \
    /*9*/  3, 3, 3, 5, 3, 3, 3, 3, 3, 3, 3, 3, 5, 5, 4, 4, \
    /*A*/  4, 4, 4, 6, 4, 4, 4, 4, 4, 4, 4, 4, 6, 6, 5, 5, \
    /*B*/  4, 4, 4, 6, 4, 4, 4, 4, 4, 4, 4, 4, 6, 6, 5, 5, \
    /*C*/  2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 2, 3,XX, 3, 3, \
    /*D*/  3, 3, 3, 5, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, \
    /*E*/  4, 4, 4, 6, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, \
    /*F*/  4, 4, 4, 6, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5 }

#endif /* CPU6803_CONST_H */
