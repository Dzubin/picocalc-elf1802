/*
 * isa_const.h - constants of the instruction-set descriptions (isa.h): what an
 * instruction does to the flow of a program, and how a processor's source text
 * writes numbers.
 *
 * Author: Thomas Dzubin
 */
#ifndef ISA_CONST_H
#define ISA_CONST_H

#define ISA_MAX_BYTES       5       /* the most bytes of an instruction the tools read (a 6809 prefix, opcode,
                                       postbyte and 16-bit offset) */

/* What an instruction does to the flow of a program (isa_flow_t). */
#define ISA_FLOW_NEXT       0       /* goes on to the next instruction (and past the
                                       skip bytes too, if it can skip)             */
#define ISA_FLOW_SKIP       1       /* always skips: goes on past the skip bytes   */
#define ISA_FLOW_BRANCH     2       /* goes to the target or on to the next one    */
#define ISA_FLOW_JUMP       3       /* goes to the target only                     */
#define ISA_FLOW_CALL       4       /* goes to the target and comes back to the next */
#define ISA_FLOW_STOP       5       /* goes nowhere the tools can follow (a return,
                                       a halt, an indirect jump)                   */
#define ISA_FLOW_DATA       6       /* not an instruction on its own: these bytes
                                       are written as data, and the code goes on   */
#define ISA_FLOW_BAD        7       /* not an instruction: data, and the code ends */

/* How numbers are written in a processor's source text (isa_t.numbers). */
#define ISA_NUM_HEX_DEFAULT     0x0001u     /* a bare number is hexadecimal            */
#define ISA_NUM_HASH_DECIMAL    0x0002u     /* #123 is decimal                         */
#define ISA_NUM_DOLLAR_HEX      0x0004u     /* $7F is hexadecimal                      */
#define ISA_NUM_H_SUFFIX        0x0008u     /* 7FH is hexadecimal                      */
#define ISA_NUM_0X              0x0010u     /* 0x7F is hexadecimal                     */
#define ISA_NUM_PERCENT_BINARY  0x0020u     /* %0101 is binary                         */
#define ISA_NUM_B_SUFFIX        0x0040u     /* 0101B is binary                         */

#endif /* ISA_CONST_H */
