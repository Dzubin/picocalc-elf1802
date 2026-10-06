# Other processors: research notes

Sources, choices made and facts not yet confirmed for the code in this folder.
The Elf's notes are in `RESEARCH.md`.

## The MC6803 core (cpu6803.c)

- Instruction semantics and flags were checked against MAME's 6800 family opcode code
  (`6800ops.hxx`, `m6801.cpp`, BSD-3-Clause, used only as a reference, nothing copied) by running both on
  896,000 random states, 4,000 for each of the 224 opcodes both define: no difference in registers, flags,
  PC or memory writes. The only opcodes they treat differently are 0x87, 0x8F, 0xC7, 0xCD and 0xCF (the
  immediate-mode stores): MAME's 6803 table does nothing or stores, this core takes the trap vector, which
  is what the data sheet says an illegal opcode does.
- Cycle counts are MAME's `cycles_6803` table (the data sheet's figures), the same as in the hardware
  manual. An interrupt takes 12 cycles, 4 after WAI. The check against the instruction-set description
  (`isa_6803`) makes sure every opcode's length agrees with the disassembler.
- The timer follows MAME: the counter counts E clocks; OCF is set when it equals the output compare
  register; TOF when it reaches $FFFF; the flags clear by reading TCSR and then the counter high byte
  (TOF), writing the compare register (OCF) or reading the capture register high byte (ICF); writing the
  counter high byte presets it to $FFF8. Reading the high byte of the counter holds the low byte for the next
  read of it (the data sheet's buffer; MAME reads the low byte live). CPX sets C on the 6803.
- Not confirmed: the exact cycle inside an instruction where the timer flags set (the core advances the
  timer after the whole instruction, as MAME does), the serial interface (only its registers exist) and
  IRQ2 priority against IRQ1 on the real chip.

## The MC6847 video chip (mc6847.c)

- Timing (data sheet, MAME): 262 lines of 228 colour clocks (the 3.58 MHz that the processor divides by four, so
  57 processor cycles a line and 14,934 a frame, 59.92 frames a second). Lines 0 to 24 are the top border, 25 to
  216 the 192 display lines, then 26 bottom border, 6 retrace and 13 blanking lines. Field sync is low from line 216
  to 247.
- The chip counts its own addresses: a text row is 32 bytes for 12 lines (16 rows); a graphics picture is 16 or 32
  bytes a row, 64, 96 or 192 rows, each row repeated for 192 / rows lines. In the alphanumeric modes bit 7 of the
  byte is the chip's A/S pin (semigraphics) and bit 6 its INV pin (inverse video).
- Semigraphics 4: bits 0 to 3 are four blocks of 4 x 6 pixels, bits 4 to 6 the colour; semigraphics 6: bits 0 to 5
  are six blocks of 4 x 4, bits 7 and 6 the colour (bit 7 is also A/S, so only two colours of each set in practice),
  CSS adds 4. Graphics with two colours use black and green (CSS low) or black and buff; four colours use colours
  0 to 3 or 4 to 7. Text is bright on dark green (CSS low) or orange; the border is black in text modes and green or
  buff in graphics.
- The palette values are MAME's (measured from a real chip). The character cell is 8 x 12 with the 5 x 7 glyph at
  column 2 and row 3, as MAME places it.
- The character shapes are drawn by hand (the chip's own character ROM, 280 bytes, is not a file this project has),
  so the letter shapes are standard 5 x 7 forms and may differ in small details from the real chip's; the arrows at
  codes 0x1E and 0x1F are the real set's up and left arrows.
- Not done: the NTSC colour artifacts of the two-colour 256-pixel mode (the picture is shown in its two palette
  colours), the 6847T1's lower case set, PAL, a mode change in the middle of a line (a change takes effect from the
  next line), HS output, and the border changing from line to line.

## The TRS-80 MC-10 (mc10.c)

- Wiring as in MAME's `mc10.cpp`: processor clock 3.579545 MHz / 4 = 894,886 cycles a second; RAM from `$4000`
  (4K, or 20K with the expansion), the video chip reads it from its start; the ROM is 8K at `$E000` (MC10.ROM,
  CRC32 11fda97e); `$BFFF` read gives the keyboard rows port 1 selects (a low bit selects a row, selected rows are
  ANDed, a key that is down reads low), write sets the video chip's mode pins from bits 2 to 6 (bit 2 is both GM2
  and INT/EXT) and the sound output from bit 7. Port 2: bit 0 cassette and printer output, bit 1 low when
  control, break or shift is down on rows 0, 2 and 7, bit 2 RS-232 in, bit 3 CTS, bit 4 cassette in. There is no
  video interrupt and no use of the video chip's HS or FS.
- The keyboard matrix is the table in `mc10_const.h` (MAME's `pb0` to `pb7` rows); the arrow keys are CONTROL with
  A, S, W or Z.
- The sound level is fed to `qaudio` once per cycle of an instruction, the old level for all but the last cycle (a
  store takes effect in the instruction's last cycle).
- Not confirmed: the real RAM layout past the installed amount (it reads as open bus here), what the video chip
  reads beyond the end of a 4K RAM in the larger graphics modes (open bus here), the NMI from the cartridge port
  (not wired) and the serial port (registers only).

## Z80 (skeleton)

- `isa_z80.c` decodes by the opcode's bit fields (x = bits 7-6, y = bits 5-3, z = bits 2-0), the layout in the Zilog Z80
  CPU User Manual (UM0080); documented instructions only. Left out as undocumented: SLL, the halves of IX and IY
  (IXH and the like), IN F,(C), OUT (C),0, the duplicate NEG, RETN and IM opcodes, ED 63 and ED 6B (the longer
  forms of LD (nn),HL and LD HL,(nn)), and the DD CB d op forms that also copy the result to a register.
- `cpuz80.c`: the T-states are the unprefixed table of the same manual (a conditional RET is 5 or 11, a conditional CALL
  10 or 17, JP cc is 10 either way); the undocumented flag bits 5 and 3 (copies of the result, or of the operand for CP,
  and of the high byte for ADD HL) and the HALT, EI and interrupt behaviour are as described in Sean Young's "The
  Undocumented Z80 Documented". The tables were written from those documents' contents as recalled, not copied from a
  file; the arithmetic is checked against an independent calculation (every 8-bit ADD, ADC, SUB, SBC, AND, XOR, OR and
  CP, INC, DEC, and DAA against BCD arithmetic) but **not yet against a published test suite**: run one (for example
  zexdoc, or the SingleStepTests Z80 vectors) before relying on it. Not confirmed: the flag bits 5 and 3 of SCF and
  CCF on a real chip (taken from the accumulator here; some chips also OR in the old F), the exact effect of EI
  followed by EI, and mode 0 with anything but a restart on the bus (only RST is run).
