/*
 * disasm1802.h - a disassembler for the CDP1802's instruction set, for the
 * debugger view. Portable ISO C.
 *
 * Author: Thomas Dzubin
 */
#ifndef DISASM1802_H
#define DISASM1802_H

#include <stddef.h>
#include <stdint.h>

/* Disassemble the instruction at addr, whose first three bytes are b[0] to
 * b[2] (those it does not use are ignored). Writes the text, such as "LDI  2F",
 * "SEP  3", "BR   0112" or "LBR  1234", into text (cut to size) and returns
 * the instruction's length in bytes, 1 to 3. A short branch shows the address
 * it goes to, which is on the page of its operand byte. Opcode 68, which the
 * 1802 does not define, shows as the INP 0 that the emulator makes of it. */
int dis1802(uint16_t addr, const uint8_t b[3], char *text, size_t size);

/* The address a branch or jump at addr goes to (a short branch's is on the page
 * of its operand byte, a long branch's is the two bytes after it), or -1 if the
 * instruction does not branch (SKP and the long skips are not counted). *length
 * is the instruction's length, 1 to 3. */
int dis1802_target(uint16_t addr, const uint8_t b[3], int *length);

/* Find the addresses that branches and jumps in the image go to, so a listing can
 * mark them. It decodes the image from address 0 to its last non-zero byte, one
 * instruction after another, and sets bit (address & 7) of marks[address >> 3]
 * for each target inside the image (marks has size / 8 bytes, cleared first).
 * Data mixed in with the code is decoded as instructions too, so it can add a
 * mark that is not a real target. */
void dis1802_mark_targets(const uint8_t *image, uint32_t size, uint8_t *marks);

/* The name of an opcode as the disassembler spells it, for the assembler. The
 * operand kind is one of the DIS_* values of disasm_const.h (nothing, a
 * register, a port, a data byte, a short or long branch address, a short skip)
 * and *reg is the register or port number the opcode itself carries (its low
 * nibble, less 8 for INP). Opcode 68, which the 1802 does not define, is
 * reported as INP with port 0. */
const char *dis1802_opcode(uint8_t op, int *operand, unsigned *reg);

#endif /* DISASM1802_H */
