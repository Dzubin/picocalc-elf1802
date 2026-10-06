# Programs from the Emma 02 collection

Raw memory images (up to 8K) for the Elf1802 **Open** menu: copy the `.BIN` files into
the `ELF1802` folder on the SD card, press `ESC`, `O`, pick one, then `R` to run
it from address 0000 (they were all made to start there).

**Where they came from:** the `data/Elf` folder of the Emma 02 emulator's
repository, [etxmato/emma_02](https://github.com/etxmato/emma_02), downloaded on
2026-10-05 (only the Elf, Netronics Elf II, Chip-8 and VIP folders, about 14 MB, kept outside the project).
Emma 02 is GPL; the programs in it keep their own copyrights (RCA, Netronics,
Quest, Paul Moews and others, or unknown), so these are included in the repository as abandonware, for your
own use, and are not covered by the project's licence. The Christmas tunes came as
Intel HEX song data; each was joined to its "Music Machine" player and turned
into a raw image so that it runs by itself.

**How they were checked** (the first 50; the VDU games, the Tiny BASICs and
`ENTERPRS` were run the same way on the VDU and the keyboard): each image was run in the emulator for 4 seconds,
once with the keypad tapped, and looked at: does the video chip come on, does
the picture change, does Q move, does the program stay inside the 16K of RAM.
That shows they start and draw something sensible. It does **not** show that
the games play correctly; none has been run on the PicoCalc yet.

## Pixie games and demos

| File | What it is | Keys and notes |
|---|---|---|
| `BREAKOUT` | Breakout | A clear frame and bricks on screen. |
| `BLOCKADE` | Two-player worm game, 1802 code | Player 1: 8 up, C down, 0 left, 4 right. Player 2: B up, F down, 3 left, 7 right. Don't hit a wall. |
| `DOGFIGHT` | Dog Fight (the VIP game, CHIP-8) | 2 up, 8 down, 4 left, 6 right, 5 fire. Plane and ground draw correctly. |
| `DOTDASH` | Dot-dash (CHIP-8): get the flashing dot across | 2/8/4/6 move, 0 slows it down. |
| `MINIGOLF` | Miniature Golf from the VIP II manual (CHIP-8) | Start with 1 to 4 for the number of players. |
| `PINBALL` | Pinball (CHIP-8) | |
| `XRAY` | X-ray (CHIP-8) | |
| `BLKJACK` | Blackjack (CHIP-8) | |
| `CURSES` | Curses! Foiled Again (CHIP-8) | |
| `SPACEINV` | Space Invaders (CHIP-8) | The collection's notes call it unfinished and buggy; it needs the EF3 and EF4 buttons, which this machine does not have. |
| `COWBOY` | Cosmac Cowboy: shoot the moving target | Any key fires. Notes say it has small bugs. |
| `COSMOS` | Cosmac Cosmos: make all nine marks show `o` | Keys 0 to F change the marks. |
| `TARGET` | Target game | |
| `PUZZLE15` | The 15 Puzzle | Uses the hex displays too. |
| `ELFINVAD` | Elf Invaders | Uses the hex display. |
| `BATTLE` | Battle | Uses the hex displays. |
| `STARPILT` | Starpilot (4,352 bytes) | Needed more than 4K; fine with 16K. |
| `INVADERS` | Invaders (6,656 bytes) | Needed more than 4K. |
| `STARTREK` | Star Trek (6,656 bytes) | Needed more than 4K. It uses the hex displays. |
| `STARTRK2` | Star Trek 2 (7,680 bytes) | The longer version. |
| `STARSHIP` | Starship | A game with a busy screen. |
| `FLIPFLOP` | Flip-flop (256 bytes) | The picture is the program's own memory, as on the original 256-byte Elf. |
| `KALEIDO` | Kaleidoscope (768 bytes) | |
| `QBLKJACK` | Quest Blackjack | |
| `ELFDIE` | Elf Die (a die on the screen) | |
| `LONGLIVE` | Long Live the 1802 | |

## The Enterprise program from the 1977 article

| File | What it is |
|---|---|
| `SPCSHIP` | **The starship program from Popular Electronics, July 1977** ("Build the PIXIE Graphic Display", Table I and Table II), 256 bytes. The 1861 shows the whole memory as dots: the program's own bytes as binary pixels at the top, then the block letters **COSMAC**, the Enterprise, and **ELF** at the bottom right. Press `R` to run; the main program turns the display on and then lets you enter bytes with the hex keypad and `I` (the first byte entered sets the address, the following ones are stored there). |

## The Enterprise picture (from the Elf 2000 ROM)

| File | What it is |
|---|---|
| `ENTERPRS` | The "famous Enterprise picture", 64 x 32 pixels with a banner above it. It is not a program of its own but a 256-byte picture in the Elf 2000's firmware ROM (offset 1500 in `Elf2K/v88.bin`, at 9500 when the ROM sits at 8000, where its CDP 1861 test shows it). This file is a small program of mine around that picture: the standard Elf video setup and an interrupt routine that shows each of the 32 rows on four lines (the 29-cycle routine plus the R0 reset trick), checked to give the picture byte for byte. |

## VDU games (need the VDU, which the machine now has)

These are CHIP-8 games for a screen at E000 to E3FF; they draw 64 x 32 pixels
with block graphics and use the hex keypad (INP 4). The top of the screen
switches to the VDU by itself when one starts. Each was run in the emulator and
draws a proper picture.

`ANIMRACE`, `BACKGAMM`, `BASEBALL`, `BIORHYTH`, `BOWLING`, `CRAPS`, `GALAXYPT`
(Galaxy Patrol), `LUNARLND` (Lunar Lander), `TANKSTRG` (Tank Struggle), `WUMPUS`,
`VDOGFGHT` (Dog Fight) and `VMINIGLF` (Miniature Golf), the VDU versions of two games
above, and `CHIP10`, the CHIP-8 interpreter that draws in 64 x 128.

## Tiny BASIC

Press `TAB` to type on the ASCII keyboard (letters come out as capitals, which
Tiny BASIC wants), and press a key to start it.

| File | What it is |
|---|---|
| `TBPIXIE` | Netronics Tiny BASIC, text drawn on the 1861 picture (4 x 5 pixel letters, small). Any key at the start selects the keyboard. |
| `TBVDU` | Tiny BASIC on the VDU: a clean 32 x 16 text screen. `PRINT 2+2` answers 4. |

## Q and sound

The Q line is played as a tone, so these are heard as well as seen (the Q LED
flickers too):

| File | What it is |
|---|---|
| `ELFDUET`, `OVERTURE`, `SONGBIRD`, `WITCHDOC` | Tiny tunes, 41 bytes each |
| `BLINKER` | Q blinking |
| `SWEEP` | A rising and falling sweep on Q |
| 18 Christmas carols | `JINGLEBE`, `SILENTNI`, `JOYTOTHE`, `HARKTHEH`, `OLITTLET`, `GODRESTY`, `THEFIRST`, `WETHREEK`, `WHATCHIL`, `OCHRISTM`, `OCOMEALL`, `GOODKING`, `DECKTHEH`, `ANGELSWE`, `AWAYINAM`, `ITCAMEUP`, `JOLLYOLD`, `WHILESHE` |

## Left out, and why

- **`monitor_editor`** (Elf/VDU): needs a monitor ROM above the RAM.
- **PCM sound:** Buttermilk Hill, Close Encounters and Happy Birthday play
  digitized sound by flipping Q thousands of times a second, which the tone
  estimator would turn into a buzz.
- **Not Elf programs:** the VIP and CHIP-8 collections (`Vip/`, `Chip-8/`, about
  360 `.ch8` games) need a CHIP-8 interpreter and, for the VIP ones, the VIP's
  ROM; they are not downloaded into this project.
- **3aplus4b** (Quest): no picture and it idles; it needs something the
  machine doesn't have.
