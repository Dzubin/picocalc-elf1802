# Changelog

All notable changes to Elf1802.
Format loosely follows [Keep a Changelog](https://keepachangelog.com/);
the version here matches the `VERSION` define in `elf1802_const.h`.

## [Unreleased]

### Changed
- The keys are reorganised so the keypad screen stays the primary one. `R`, `L`, `M` and `I` are the switches and IN
  (the function keys no longer do that), `V` shows the VDU or the picture, and **`ESC` is the one way to everything else**:
  it opens an arrow-key menu with the groups FILES (SD CARD) (save, open, new, write a listing, ROM images), TOOLS (editor, debugger, help)
  and MACHINE (memory protect, keyboard, sound, picture or VDU, the 1861) and the title screen. Menu letters
  are never keypad keys (the old `C`, `D`, `E`, `R` rows moved; the 1861 is `Y`, the title screen `T`).
- Only the unshifted function keys are used now: `F1` is help (also `H` and `?`), `F4` switches the ASCII keyboard on
  and off (the same key both ways; in ASCII mode `R L M I V H` are typed to the program), `F5` sends ESC to the
  program. `F6` to `F10` (the view, debugger, sound, help and ASCII keyboard keys) and `F1` to `F4` as switches are gone;
  their functions are in the menu. The tag under the Q LED reads `ASCII` over `F4=OFF` in ASCII mode and the hint pill
  says `KBD?` over `F4`.
- Clearing the RAM from the menu asks first (`Y` to confirm). The title screen reminds you that `ESC` is the menu and `F1` is help.
- The editor's menu and the ROM screen work the same arrow-key way. The editor menu has two new rows, **go** (assemble and
  run) and **debug** (assemble and open the debugger), the debugger has `T` for the editor, and `F1` shows help in the
  editor (new editor help screen).
