# Elf1802 research notes

What the emulator is built from: the RCA CDP1802 microprocessor, the CDP1861
"Pixie" video chip, and the Netronics Elf II front panel. Each fact below says
where it came from, and the last section lists what is **not** yet confirmed
against a source or a real machine.

## The CDP1802 (COSMAC)

From the Intersil/RCA CDP1802A data sheet:

- Sixteen 16-bit registers R0 to R15, an 8-bit accumulator D, a carry flag DF,
  4-bit P (which register is the program counter), X (which is the data
  pointer), N and I (the two halves of the opcode), an 8-bit T (holds X and P
  after an interrupt), IE (interrupt enable) and Q (a one-bit output).
- Four flag inputs EF1 to EF4 tested by the short branches. A flag is **true when
  its pin is low**: the instruction table reads "short branch if EF1 = 1 (EF1 =
  VSS)" for B1 and "EF1 = 0 (EF1 = VCC)" for BN1, so B1 to B4 branch on a low pin
  and BN1 to BN4 on a high one. (The first version of the emulator had this
  backwards; the 1861 display routines of several real programs then ran wrong,
  and Star Trek's screen came out garbled, until the "Build the PIXIE Graphic
  Display" article's own listing showed the mistake.) They are sampled at the
  start of each S1 cycle.
- A machine cycle is **8 clock pulses**. Most instructions take two machine
  cycles (S0 fetch, S1 execute); long branches, long skips and NOP take three
  (S0 and two S1). S2 is a DMA cycle, S3 the interrupt response.
- Control modes from the CLEAR and WAIT pins: LOAD (both low), RESET (CLEAR
  low, WAIT high), PAUSE (CLEAR high, WAIT low), RUN (both high).
- Reset clears I, N and Q and sets IE. The first machine cycle after reset
  ends is an initialization cycle of **9 clocks** that clears X, P and R0; the
  cycle after it can be S0, S1 or S2 but never S3.
- DMA uses R0 as the memory pointer and increments it. Priority when
  requests coincide: DMA-IN, then DMA-OUT, then INTERRUPT. Requests are
  sampled between TPB and the next TPA, so they are acted on after the
  current instruction (never inside the second S1 of a long instruction).
- Interrupt action (S3): T = (X,P), X = 2, P = 1, IE = 0. Only with IE set.
- Load mode holds the CPU idle with no TPA or TPB; a DMA-IN request does not
  start execution.
- Instruction timing summary: 2 machine cycles for everything except the
  Cx group (LBR, LBQ, LBZ, LBDF, NOP, LSNQ, LSNZ, LSNF, LSKP, LBNQ, LBNZ, LBNF,
  LSIE, LSQ, LSZ, LSDF), which take 3.

Instruction semantics (the opcode map, the exact carry and borrow rules, MARK,
SAV, RET, DIS and so on) are as in the data sheet's instruction table and the
well-known COSMAC programming references; `tests/test_emulator.c` checks them.

## The CDP1861 (Pixie)

From the emma02 CDP1861 page, the MAME 1861 source and the Hackaday FPGA Elf
notes:

- 64 pixels across and up to 128 lines down, 1 bit per pixel, taken from RAM by
  DMA. A program can show fewer lines (64x64, 64x32) by pointing R0 back at the
  same bytes, so a 256-byte picture is the classic 64x32.
- **14 machine cycles per video line, 262 lines per frame.** On each picture
  line 8 cycles are DMA (one byte each) and 6 are left for the CPU, which is
  exactly three two-cycle instructions.
- The chip interrupts the CPU so that DMA starts "in exactly 29 cycles"; the
  program's interrupt routine must set R0 to the start of the picture and use
  up an even number of cycles. A three-cycle instruction in the main program
  breaks the timing (the interrupt routine begins with a compensating NOP).
  The interrupt request is cleared 28 cycles after it was asserted.
- EFX (EF1 on the Elf) goes low in the last four lines before the picture and
  in the last four lines of it.
- MAME models the chip with a DMA start 2 machine cycles into the line, 8
  active and 6 waiting.
- Display on and off: INP 1 turns the picture on, OUT 1 turns it off (the
  Elf's port decode, from the VIP and Elf documentation).

How the emulator uses it: `cdp1861.c` counts lines and cycles with the CPU.
INT is low from the start of line 78 to the start of line 80 (28 cycles); DMA
is requested on cycles 2 to 9 of lines 80 to 207; EFX is low on lines 76 to 79
and 204 to 207. A DMA byte is stored at the slot given by the cycle it arrived
in (cycle 2 is the first byte of the line), so a program that is a cycle out of
step gets a shifted picture, as on the real chip. `tests/test_emulator.c` runs
the standard 29-cycle interrupt routine and checks the picture comes out
byte for byte. (When the main program happens to be a cycle out of step with
the first interrupt, that first frame is skewed by a byte and the routine's
compensating NOP puts the following frames right, as on the real chip.)

## The Netronics Elf II front panel

From the emma02 Elf II pages (panel, keyboard, I/O map):

- LOAD up and RUN down is load mode; RUN up and LOAD down is run mode, which
  starts at address 0. (Both down is reset, both up is pause, since RUN is the
  CLEAR line and LOAD is the WAIT line turned over.)
- MP (memory protect) up disables memory writes.
- A hexadecimal keypad built on a 74C923 encoder and a 74C173 latch keeps the
  **last two keys**; INP 4 returns them as one byte (previous key in the high
  nibble). In load mode the IN button starts a DMA-IN that stores that byte at
  R0 and increments R0. EF4 reads 0 while IN is pressed.
- Two hex displays show the byte written by OUT 4. A single Q LED shows the Q
  flag. Elf II memory is 256 bytes as sold; this emulator has 16K (`ELF_RAM_SIZE`).
- **Port 4 (the hex keypad in, the two hex displays out).** `INP 4` returns the
  keypad's two-digit latch (the last two digits typed, the older one in the high
  nibble; 00 before any key), `OUT 4` writes the displays, which hold the byte until
  it is written again; in LOAD mode the displays also follow IN. This is the
  standard Elf wiring that the Elf programs this emulator was tried with (and the
  Popular Electronics and Netronics material) rely on, but the circuit itself (the display latches, the key
  encoder) was not read from a schematic for this emulator. Documented for
  programmers in the README ("Programming the Elf").
- A basic Elf (no 1861 fitted, `elf_set_video(m, false)`): EF1 reads high, since with
  no chip nothing drives it (an Elf II without the chip has an unused flag pin), and
  there is no interrupt, no DMA and no response on port 1. A real Elf that had no
  video board was the original (Popular Electronics, 1976) design; the choice here is
  just what an unconnected EF1 and a missing chip must do.
- RESET turns the 1861's picture off (the chip's RESET pin is thought to be on the
  Elf's CLEAR line; not read from a data sheet), so a program that is opened and started
  begins with the display off and turns it on itself. Without this the chip stayed
  on from the previous program. Unconfirmed.
- The Q sound is not a model of any particular speaker circuit: Q's level on every
  machine cycle is averaged into 22,050 samples a second, the slow average is
  taken out (so a steady Q is silent, as through the coupling capacitor of a real
  speaker driver) and the rest is played. Choices: the DC blocker's time constant
  (about 6 ms) and the volume (`QAUDIO_DC_SHIFT`, `QAUDIO_VOLUME`) were picked by
  ear and not yet heard on the hardware. A program that flips Q faster than
  11 kHz is averaged (the box filter), not aliased.
- The clock is the 3.579545 MHz crystal divided by two (about 1.79 MHz), which
  is what makes the 1861 line rate match a TV (Wikipedia, COSMAC Elf).

## Choices made where the sources are silent

| Question | Choice | Where |
|---|---|---|
| IN button on EF4 | low while pressed (it grounds the pin) | `elf.c` |
| ASCII keyboard: a latch, EF3 low until INP 7 reads it, next key 20 ms later | chosen so both Tiny BASICs work | `elf.c`, `ELF_KBD_GAP_CYCLES` |
| VDU: 1K at E000, bit 6 = block graphics, bit 7 = inverse, bits 4 and 5 = colour | read from how the VDU games and Tiny BASIC write it | `VDU_*` in `elf_const.h` |
| Value read from an address with no memory | 0x00 | `ELF_UNMAPPED_READ` |
| Value read by INP from a port with no device (and what INP 1 stores) | 0x00 | `ELF_FLOATING_BUS` |
| Unused EF2 | held high | `ELF_EF_UNUSED` |
| Opcode 68 (not on the 1802) | acts as INP 0 | `io_group()` in `cpu1802.c` |
| Displays in load mode | follow the keypad, then the byte loaded | `elf.c` |
| EFX while the display is off | still toggles | `cdp1861.c` |
| A DMA cycle while idling in RUN mode (after IDL) | the CPU stays idle | `cpu1802.c` |

## Not confirmed yet

These are from memory or inference, not from a page or a real Elf, and are the
first things to check against a real program or a reference emulator:

1. The exact 1861 line numbers (INT at 78 and 80, picture from 80 to 207, EFX at
   76 and 204). They are consistent with the 28-cycle interrupt and the
   29-cycle gap above, and match MAME's constants as remembered, but they were
   not read from a source.
2. Whether the chip's EFX is gated by the display being on.
3. What a real Elf's data bus shows on an INP from nothing, and what 68 does.
4. Whether a DMA cycle ends an IDL in RUN mode (the data sheet's state
   diagram was read as "no").
5. Elf II hex display behaviour in load mode.
6. The 1861 pixel aspect (the screen shows 64x128 pixels as 5 wide by 1 tall,
   a TV-like 2.5:1 picture).
7. That RESET turns the 1861's picture off, and that a basic Elf (no 1861) leaves
   EF1 high.
8. The Q sound's volume and DC-blocker time constant (chosen by ear for a small
   speaker, `QAUDIO_VOLUME` and `QAUDIO_DC_SHIFT`).

## The ASCII keyboard and the VDU (found by running the software)

The Tiny BASIC and CHIP-8 images in the Emma 02 collection were written for an
Elf with an ASCII keyboard and a memory-mapped video board, which no data sheet
here describes, so the behaviour was worked out from the programs themselves and
checked by running them:

- Both Tiny BASICs wait for EF3 low (`BN3` loops while it is high) and then read
  INP 7. The VDU one reads port 7 **twice** and compares, so the code must stay
  readable after the first read; the emulator keeps it in a latch.
- The Netronics Tiny BASIC holds the display while EF4 is true (`B4 HOLD`), so the
  IN button must pull EF4 low, and it is high otherwise.
- The VDU games and Tiny BASIC store into E000 to E3FF with the values 40 to 4F
  (a block cell: bit 6 set, the low nibble the four blocks), and 01 to 3F (text,
  6847 character codes, A = 01, space = 20), A0 for the cursor. Their screens come
  out right (WUMPUS, Lunar Lander and the others draw proper pictures). Which
  colour a block cell has (bits 4 and 5) is a guess (green, yellow, blue, red).
- The memory-sizing loop at the start of Tiny BASIC stops at the end of the
  16K of RAM, because writes above it are ignored and reads give 00.

Not confirmed: that the real board decoded exactly 1K (the programs only use
E000 to E3FF) and the real keyboard's timing between keys.

## The 1861 program in the original article

The July 1977 Popular Electronics article "Build the PIXIE Graphic Display" (Elf part
4) lists, in Table I, a 64-byte test program that displays the whole 256 bytes of
memory on the screen (each memory bit a dot, 64 x 32), and in Table II the "spaceship
program" bytes at 0040 to 00FF: block letters COSMAC and ELF and the starship. Typed
in together they make the 256-byte program with its own bytes as dots on top and the
Enterprise below. It uses `B1` and `BN1` on the 1861's EFX flag exactly as the
corrected CPU treats them; `programs/emma02/SPCSHIP.BIN` is that program, rebuilt
from the OCR of the article (the main program's bytes were put back in order from
the text and its branch targets, the data checked against the picture it draws).

## Sources

- RCA/Intersil CDP1802A, CDP1802AC, CDP1802BC data sheet (March 1997).
- emma02: [CDP1861](https://emma02.hobby-site.com/cdp1861.html),
  [Elf II panel](https://emma02.hobby-site.com/elf2_panel.html),
  [Elf II keyboard](https://emma02.hobby-site.com/elf2_keyboard.html),
  [Elf II I/O map](https://emma02.hobby-site.com/elf2_iomap.html).
- Wikipedia: [RCA CDP1861](https://en.wikipedia.org/wiki/RCA_CDP1861),
  [RCA 1802](https://en.wikipedia.org/wiki/RCA_1802),
  [COSMAC Elf](https://en.wikipedia.org/wiki/COSMAC_Elf).
- MAME `src/devices/video/cdp1861.cpp` (cycle constants).
- Hackaday.io, FPGA COSMAC Elf project (DMA timing notes).
- Popular Electronics, July 1977, "Build the PIXIE Graphic Display" (Table I and Table II),
  from worldradiohistory.com.
