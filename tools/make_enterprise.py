# Builds programs/emma02/ENTERPRS.BIN: a small 1861 program around the 256-byte
# Enterprise picture that sits in the Elf 2000's ROM image (offset 1500).
#
#     python make_enterprise.py path/to/v88.bin
import os
import sys

if len(sys.argv) != 2:
    sys.exit("usage: python make_enterprise.py path/to/v88.bin")
SRC = sys.argv[1]
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "programs",
                   "emma02", "ENTERPRS.BIN")

rom = open(SRC, "rb").read()
picture = rom[0x1500:0x1600]            # 256 bytes: 64 x 32 pixels
assert len(picture) == 256

ISR = 0x0020                            # R1 points here; the RET is the byte before
MAIN = 0x0018                           # R3, the main program
PIC = 0x0400                            # where the picture sits (RF points at it)
STACK = 0x00FF

boot = bytes([
    0xF8, 0x00, 0xB1, 0xF8, ISR, 0xA1,              # R1 = interrupt routine
    0xF8, 0x00, 0xB2, 0xF8, STACK, 0xA2,            # R2 = stack
    0xF8, 0x00, 0xB3, 0xF8, MAIN, 0xA3,             # R3 = main program (0018)
    0xF8, PIC >> 8, 0xBF, 0xF8, PIC & 0xFF, 0xAF,   # RF = the picture
    0xE2,                                           # X = 2
    0x69,                                           # INP 1: display on
    0xD3,                                           # P = 3
])
# boot is 27 bytes: 0000-001A; the main program goes at 001B, so move MAIN
MAIN = len(boot) + 0
main_loop = bytes([0x30, MAIN])                     # BR *

img = bytearray(4096)
boot = bytearray(boot)
boot[16] = MAIN                                     # LDI MAIN for R3.0
img[0:len(boot)] = boot
img[MAIN:MAIN + 2] = main_loop

loop = ISR + 22                                     # where the triplets start (see below)
isr = bytearray([
    0x70,                                           # ISR-1: RET (ends the last interrupt)
    0xC4,                                           # NOP, 3 cycles
    0x22, 0x78, 0x22, 0x52,                         # DEC 2, SAV, DEC 2, STR 2
    0x9F, 0xB0, 0x8F, 0xA0,                         # R0 = RF
    0xC4, 0xC4,                                     # two NOPs
    0xE2, 0x80,                                     # SEX 2, GLO 0 : 29 cycles in all
])
loop = ISR - 1 + len(isr)
exit_at = loop + 14
isr += bytearray([
    0xE2, 0x20, 0xA0,                               # three gaps that put R0 back,
    0xE2, 0x20, 0xA0,                               # so each row is shown on
    0xE2, 0x20, 0xA0,                               # four lines
    0x34, exit_at & 0xFF,                           # B1: EF1 is true (low), the picture is over
    0x80,                                           # GLO 0: the next row's start
    0x30, loop & 0xFF,                              # BR to the first gap
    0x42, 0x30, (ISR - 1) & 0xFF,                   # exit: LDA 2 (D back), BR to the RET
])
assert exit_at == ISR - 1 + len(isr) - 3, (exit_at, ISR - 1 + len(isr) - 3)
img[ISR - 1:ISR - 1 + len(isr)] = isr
img[PIC:PIC + 256] = picture
open(OUT, "wb").write(img)
print("wrote", OUT, "boot", len(boot), "main", hex(MAIN), "isr", hex(ISR), "loop", hex(loop), "exit", hex(exit_at))
