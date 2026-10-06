/*
 * othercpus.h - everything in this folder, for a program that wants the other
 * processors and machines (the Elf's firmware and tests do not include it): the
 * instruction-set descriptions of the 6502, 8080, Z80, 6800, 6803 and 6809, the
 * MC6803 and Z80 CPU cores, the MC6847 video chip and the TRS-80 MC-10. Add this
 * folder to the include path (-Iothercpus) and compile the files you use; the
 * commands are in othercpus/README.md. None of it is part of the Elf build.
 *
 * Author: Thomas Dzubin
 */
#ifndef OTHERCPUS_H
#define OTHERCPUS_H

#include "cpu6803.h"
#include "cpuz80.h"
#include "isa_others.h"
#include "mc10.h"
#include "mc6847.h"

#endif /* OTHERCPUS_H */
