# Changelog of the other processors

Changes to the code in this folder (see `../CHANGELOG.md` for the Elf).

## [Unreleased]

### Added
- The Zilog Z80A, as the basics for later work: `isa_z80.c` describes every documented opcode (assembler with
  Zilog mnemonics, disassembler, flow for the listing; checked by `tests/test_isa_others.c`), and `cpuz80.c` is a CPU
  core skeleton that runs the 8080's instruction set with the Z80's flags and T-states, HALT, EI's delay, R and the
  interrupts; the rest (JR group, EX AF,AF', EXX, the CB, DD, ED and FD groups) is flagged `unsupported`.
  `tests/test_cpuz80.c` checks it. Nothing uses it yet.
- `mc10.c`: the TRS-80 MC-10 as a machine (6803 + memory map + 6847 + keyboard matrix + one-bit sound + cassette
  lines, ROM fitted from a `.ROM` file, a built-in "no ROM" message program); `tests/test_mc10.c` checks it. No
  front end yet. `STATUS.md` has the hand-off notes and the plan.
- `mc6847.c`: the Motorola MC6847 video chip (text, semigraphics, all graphics modes, line-accurate frame
  timing, changed-line marks), the second piece of the TRS-80 MC-10; `tests/test_mc6847.c` checks it. Nothing
  uses it yet.
- `cpu6803.c`: a Motorola MC6803 CPU core (full instruction set, data sheet cycle counts, interrupts, the
  on-chip RAM, ports and timer), the first piece of the TRS-80 MC-10. Nothing uses it yet; `tests/test_cpu6803.c`
  checks it.

### Changed
- Found in a code review of the MC-10 work: SHIFT is held for as long as any shifted symbol is down (and
  not let go with a symbol while it is held on its own); `CLR` of a memory byte reads it first, as the
  chip does (it matters at the on-chip registers); the 6803 timer and the sound advance a whole instruction
  at a time (`qaudio_cycles()`) with the same result as cycle by cycle; the video chip ignores a line
  number outside 0 to 191; palette indices and register masks are named constants.
