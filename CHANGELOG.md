# Changelog

All notable changes to Elf1802.
Format loosely follows [Keep a Changelog](https://keepachangelog.com/);
the version here matches the `VERSION` define in `elf1802_const.h`.

## [Unreleased]

- Updated the vendored `picocalc-text-starter` to Blair Leduc's code of 2026-10-18 (commit
  `85a002e`) and moved to Pico SDK 2.3.1. The search-hint, deleted-entry and busy-card fixes this
  copy carried are in his code now; four small local fixes remain (README, Third-party). The audio
  driver in it now sounds 20 to 20000 Hz, where it used to be silent outside 100 to 2000 Hz.
- The Membership Card's monitor (`programs/MCMON.ASM`) is now a real one: `D hhhh` dumps 64 bytes,
  `M hhhh` puts bytes in memory and `G hhhh` runs a program, with a prompt, upper or lower case
  and `?` for the list. It uses a small call and return convention of its own (SEP 4 and SEP 5),
  so its routines can call each other. `tests/test_mcard.c` types to it like a person (73 checks).

## [V0.20A] - 2026-10-15

- The 1802 Membership Card board ([Lee Hart's project](https://www.sunrise-ev.com/1802.htm); `mcard.c`, `mcard_const.h`; checked by `tests/test_mcard.c`),
  Lee Hart's Rev L: a 4 MHz 1802, 32K RAM at 0000, ROM images from 8000, the serial line on Q and
  EF3 (4800 baud, Q inverted), eight data switches (INP 4), data LEDs and hex display (OUT 4), the
  IN button on EF4 and in LOAD mode, WAIT, CLR and READ/WRITE switches, and the Rev L jump to
  8000 when IN is held as CLR goes up. It is a second machine and does not touch the Elf II; the
  screen and keys for it (`mcardui.c`, `C` on the title screen) show the terminal on top and the
  front panel under it; it has a tiny serial monitor of its own in ROM at 8000 (`programs/MCMON.ASM`).
- Title screen: `N` chooses the Netronics Elf II explicitly (any other key still does); `ESC` then `T`
  in either machine goes back to it.
- Quick wins for a bigger machine:
  - The menu's `Z` row chooses the RAM size, 16K, 32K or 64K (64K at power-on). Memory above the
    size is not mapped (reads give 00, writes are ignored), so old programs that probe for the end
    of memory can be run as on a 16K Elf. A power-on with `B` on the title screen keeps the size.
  - On the RP2350 and in the desktop builds the assembler, the listing and the debugger's branch
    marks now work on all 64K (`ASM_IMAGE_SIZE`); the RP2040 keeps 16K because of its RAM.
  - `.HEX` files are read a piece at a time (`hex_stream_*`, `plat_file_open/read/close`), so
    their length no longer matters and the 48 KB buffer is gone (44 KB of RAM saved).
  - `U` in the menu shows the memory map: runs of RAM, ROM (with the image's name), devices
    (the VDU) and empty space (`memview.c`).
  - A serial line on Q (out) and EF3 (in): menu row `J` turns it on at 4800, 9600, 2400, 1200 or
    300 baud (8 data bits, no parity, one stop bit) and a row under it turns Q's polarity round
    (inverted is the 1802 Membership Card from Rev J; direct is before it). Characters the
    program sends are shown in a 40 x 16 terminal in the top of the screen (`terminal.c`); the
    ASCII keyboard (F4) types down the line. EF3 is the line while it is on, not the keyboard
    strobe. It is off at power-on, so the Elf II behaves exactly as before.

- The Elf's RAM is now 64K (0000 to FFFF) instead of 16K, so a ROM image can be fitted at any
  address, such as a BIOS at F000. The VDU's video RAM still sits over E000 to E3FF. The
  assembler, the listing and the debugger's branch marks still work on the first 16K
  (`SRCGEN_WINDOW`, `ASM_IMAGE_SIZE`). Save writes the RAM up to its last non-zero byte
  (a whole number of 256-byte pages, at least one) instead of always writing everything.
  Programs that look for the end of memory now find 64K.

- Vendored starter: `fat32.c` `fat32_write()` no longer leaks a cluster each time a write starts on a
  cluster boundary. The program itself is unchanged; not rebuilt.

- Vendored starter: `fat32.c` skips deleted directory entries before long-name parts (an
  out-of-bounds stack write) and `sdcard.c` waits for a busy card by the clock and before
  every command (failed reads and writes on a card that was still programming). The
  program itself is unchanged; not rebuilt.
