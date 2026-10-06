# Status of the other processors and machines (hand-off notes)

Everything here is host-tested only and not in any build. It was set aside on 2026-10-06 so the work can focus
on the Elf 1802; the Elf's own notes are in `../docs/PROJECT-STATUS.md`.

## What is finished and tested (host, 2026-10-05 and 06)

| Piece | Files | Test (command in each test's header; `sh othercpus/run_tests.sh` runs all) |
|---|---|---|
| Processor descriptions (6502, 8080, Z80, 6800, 6803, 6809) | `isa_*.c` | `tests/test_isa_others.c` 9,090 checks, `tests/vector_check.c` (113k 6502 vectors) |
| MC6803 CPU core | `cpu6803.c/.h` | `tests/test_cpu6803.c` 1,030 checks; also 896,000 random cases against MAME's opcode code, no differences (`tools/mame_diff`) |
| MC6847 video chip | `mc6847.c/.h` | `tests/test_mc6847.c` 2,906 checks |
| TRS-80 MC-10 machine core | `mc10.c/.h` | `tests/test_mc10.c` 181 checks |
| Z80 CPU core skeleton (8080 subset with Z80 flags and T-states) | `cpuz80.c/.h` | `tests/test_cpuz80.c` 1,579 checks |

## The goal (agreed 2026-10-05)

Emulate the **TRS-80 MC-10 first** (MC6803), then the **TRS-80 Color Computer** (MC6809 + SAM + PIAs + 6847).
Answers given: no ROM files yet (Thomas supplies them later as `.ROM` files, 8 KB `MC10.ROM` at 0xE000 for the
MC-10); instruction-exact timing; media: cassette loading, CoCo cartridges, sound and joysticks.

## What is left, in order

1. **MC-10 front end** (the next job). The machine core exists; nothing shows it on the PicoCalc yet.
   - A screen module that draws the 6847's lines: for each `mc6847_take_dirty(y)` line, `mc6847_render_line()` gives
     256 palette indices (`M6847_PALETTE` has the RGB values), blit them centred on the 320 x 320 LCD with the
     `mc6847_border()` colour round them. A change of border colour needs a full redraw. No dim greys.
   - The main loop in `elf1802.c` is written around the Elf; either give it a machine choice (title screen or
     menu) or add a second shell. The `machine_t` interface (`mc10_machine`) already exists for pacing.
   - Keys: `mc10_press_char()` for PicoCalc key events (CONTROL is `MC10_KEY_CONTROL`, BREAK `MC10_KEY_BREAK`,
     Enter `MC10_KEY_ENTER`); the PicoCalc has no key-up for some keys, so decide on a press-and-release scheme.
   - Sound: `m.audio` is a `qaudio_t` at `MC10_CPU_HZ`; the front end drains it with `qaudio_read()` into
     `plat_audio_play()` like the Elf's Q sound.
   - Editor/menu support: `program_target_t` now has a `base` and the assembler works in the window
     (`asm_assemble(isa, a, base, size, ...)`, buffer `ASM_IMAGE_SIZE` = 20 KB), so the editor assembles at the real
     addresses (an MC-10 target sets base `0x4000`). Still to do for a target whose RAM is not at 0: `source_gen.c`
     (the listing of the RAM, `srcgen_*`) and the debugger start at address 0 and print addresses from 0; give them
     a base too. `isa_6803` is the processor for both.
   - RP2040 RAM: the Elf build already uses about 224 KB of 264 KB; `mc10_t` is about 27 KB (20 KB RAM, VDG records 6 KB).
     Check it fits, or leave the 20 KB option to the RP2350.
2. **Cassette** (CLOAD/CSAVE, instruction-exact). Port 2 bit 4 is the input (`mc10_set_cassette_in()`), bit 0 the
   output (`m.cassette_out`). Plan: read `.CAS`/`.C10` or `.WAV` from the SD card and synthesise the input line level
   by cycle count (1200 Hz and 2400 Hz tones, 1200 baud as on the CoCo), and record the output into a file.
3. **MC-10 checks with a real ROM** once Thomas supplies `MC10.ROM` (CRC32 11fda97e): BASIC prompt, typing, graphics
   modes, sound. Compare against MAME's behaviour for anything odd.
4. **MC6809 core** (`cpu6809.c`), tested like the 6803 (tables in `isa_6809.c` exist for lengths and mnemonics;
   `RESEARCH.md` explains the MAME differential method: build MAME's opcode file into a scratch C++ harness and
   compare on random states).
5. **CoCo machine**: SAM (6883), two PIAs (6821) for keyboard, joysticks, sound, cassette, and the 6847 hookup with
   its HS/FS interrupts (`mc6847_field_sync()` exists; HS is not done), cartridges as ROM images at 0xC000, joystick
   input from the PicoCalc keys. Disk BASIC later.

## Decisions and facts worth keeping

- MAME is used only as an independent reference (BSD-licensed facts: opcode tables, cycle counts, palette, timing);
  nothing is copied in except the figures cited in `RESEARCH.md`. The MC6847 character shapes are hand drawn
  because the chip's ROM is not available.
- The MC-10 has no VDG interrupt (its timing comes from the 6803 timer); `$BFFF` read = keyboard rows, write = video
  mode pins (bits 2 to 6) and sound (bit 7); port 1 is the row strobe, port 2 carries the cassette and the
  control/break/shift line.
- The MC-10 fallback program (page 0xFF00) shows "NO MC-10 ROM FITTED / ESC R: FIT MC10.ROM AT E000"; any fitted ROM
  replaces it. ROM images are fitted with the file menu's `R` key and remembered in `ROMS.CFG`.
- The 6803 timer and the sound advance a whole instruction at a time (`tick()` in `cpu6803.c`, `qaudio_cycles()`); tests
  compare them with the cycle by cycle result. Keys: the matrix is rebuilt from `plain[]` and `symbol[]` so SHIFT is shared.
- The 6803 core takes the illegal-opcode trap vector (0xFFEE), unlike MAME's 6803 which ignores such opcodes.
