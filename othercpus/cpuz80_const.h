/*
 * cpuz80_const.h - constants of the Zilog Z80 (cpuz80.c): the flag bits, the
 * interrupt addresses, the values after reset and the clock states (T-states)
 * each opcode takes.
 *
 * Author: Thomas Dzubin
 */
#ifndef CPUZ80_CONST_H
#define CPUZ80_CONST_H

/* The flag register. Bits 5 and 3 are not documented: they copy bits of the
 * result (or of the operand, for CP), and programs that test them exist. */
#define Z80_F_C     0x01u       /* carry                                      */
#define Z80_F_N     0x02u       /* the last operation was a subtraction        */
#define Z80_F_PV    0x04u       /* parity (logic) or overflow (arithmetic)     */
#define Z80_F_X     0x08u       /* undocumented: bit 3 of the result           */
#define Z80_F_H     0x10u       /* half carry                                  */
#define Z80_F_Y     0x20u       /* undocumented: bit 5 of the result           */
#define Z80_F_Z     0x40u       /* zero                                        */
#define Z80_F_S     0x80u       /* sign                                        */
#define Z80_F_XY    (Z80_F_X | Z80_F_Y)

/* Opcode fields and values the core refers to by name. */
#define Z80_OPEN_BUS            0xFFu   /* what a read that nothing answers gives     */
#define Z80_R_MASK              0x7Fu   /* the bits of R that count fetches           */
#define Z80_SIGN8               0x80u
#define Z80_HALF_BIT16          0x1000u /* the carry out of bit 11, for ADD HL        */
#define Z80_REG_HL_INDIRECT     6       /* register number 6 in an opcode is (HL)     */
#define Z80_OP_HALT             0x76u

/* The arithmetic and logic group, by its number in the opcode (bits 5-3). */
#define Z80_ALU_ADD     0
#define Z80_ALU_ADC     1
#define Z80_ALU_SUB     2
#define Z80_ALU_SBC     3
#define Z80_ALU_AND     4
#define Z80_ALU_XOR     5
#define Z80_ALU_OR      6
#define Z80_ALU_CP      7

/* After a reset PC is 0, the interrupts are off in mode 0, I and R are 0, and AF
 * and SP are all ones. */
#define Z80_RESET_PC        0x0000u
#define Z80_RESET_AF        0xFFFFu
#define Z80_RESET_SP        0xFFFFu

/* Interrupts: NMI goes to 0066H; in mode 1 a maskable interrupt goes to 0038H; in
 * mode 2 the vector is read from the table at I * 256 + the byte the device puts
 * on the bus. In mode 0 the device puts an instruction on the bus; only the RST
 * instructions are run here (the byte, with its bits 7, 6, 2, 1, 0 ignored). */
#define Z80_NMI_ADDRESS     0x0066u
#define Z80_IRQ_ADDRESS     0x0038u
#define Z80_RST_MASK        0x38u
#define Z80_VECTOR_MASK     0xFEu
#define Z80_IRQ_DATA_IDLE   0xFFu       /* what the data bus holds with nobody driving it */

/* T-states of an interrupt: NMI 11, mode 0 and 1 13, mode 2 19. */
#define Z80_T_NMI           11
#define Z80_T_IRQ_RST       13
#define Z80_T_IRQ_MODE2     19
#define Z80_T_HALTED        4       /* a halted Z80 runs NOPs                      */

/* How the opcodes are counted. */
#define Z80_T_UNSUPPORTED   4       /* an opcode this core does not run yet is a NOP */
#define Z80_T_RET_TAKEN     6       /* extra for RET cc when it returns            */
#define Z80_T_CALL_TAKEN    7       /* extra for CALL cc when it calls             */

/* The group of conditions (opcode bits 5-3 of RET cc, JP cc and CALL cc). */
#define Z80_COND_NZ 0
#define Z80_COND_Z  1
#define Z80_COND_NC 2
#define Z80_COND_C  3
#define Z80_COND_PO 4
#define Z80_COND_PE 5
#define Z80_COND_P  6
#define Z80_COND_M  7

/* T-states of each unprefixed opcode, by opcode (rows of 16). A conditional RET
 * or CALL is given for the case that it does not take the branch. 0 marks an
 * opcode this core does not run yet: JR, DJNZ, EX AF,AF', EXX and the four prefix
 * bytes (CB, DD, ED, FD), which are the Z80's own and not part of the 8080. */
#define CPUZ80_TSTATES { \
    /* 0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */ \
        4, 10,  7,  6,  4,  4,  7,  4,  0, 11,  7,  6,  4,  4,  7,  4, \
        0, 10,  7,  6,  4,  4,  7,  4,  0, 11,  7,  6,  4,  4,  7,  4, \
        0, 10, 16,  6,  4,  4,  7,  4,  0, 11, 16,  6,  4,  4,  7,  4, \
        0, 10, 13,  6, 11, 11, 10,  4,  0, 11, 13,  6,  4,  4,  7,  4, \
        4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4, \
        4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4, \
        4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4, \
        7,  7,  7,  7,  7,  7,  4,  7,  4,  4,  4,  4,  4,  4,  7,  4, \
        4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4, \
        4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4, \
        4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4, \
        4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4, \
        5, 10, 10, 10, 10, 11,  7, 11,  5, 10, 10,  0, 10, 17,  7, 11, \
        5, 10, 10, 11, 10, 11,  7, 11,  5,  0, 10, 11, 10,  0,  7, 11, \
        5, 10, 10, 19, 10, 11,  7, 11,  5,  4, 10,  4, 10,  0,  7, 11, \
        5, 10, 10,  4, 10, 11,  7, 11,  5,  6, 10,  4, 10,  0,  7, 11 }

#endif /* CPUZ80_CONST_H */
