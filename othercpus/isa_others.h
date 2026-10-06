/*
 * isa_others.h - the instruction-set descriptions (isa_t, see isa.h) of the
 * processors other than the 1802: the 6502, 8080, Z80, 6800, 6803 and 6809. They
 * are used by the tests and by the machines in this folder; the Elf does not use
 * them.
 *
 * Author: Thomas Dzubin
 */
#ifndef ISA_OTHERS_H
#define ISA_OTHERS_H

#include "isa.h"

extern const isa_t isa_6502;
extern const isa_t isa_8080;
extern const isa_t isa_z80;
extern const isa_t isa_6800;
extern const isa_t isa_6803;
extern const isa_t isa_6809;

#endif /* ISA_OTHERS_H */
