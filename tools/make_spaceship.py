# Rebuilds programs/emma02/SPCSHIP.BIN, the 256-byte starship program of the July
# 1977 Pixie article (Table I and Table II), from the bytes below.
#
#     python make_spaceship.py
import os

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "programs",
                   "emma02", "SPCSHIP.BIN")

# Table I, the test program (0000 to 003F), from the July 1977 article
table1 = bytes.fromhex(
    "90B1B2B3B4F82DA3"     # 0000  R1.1 to R4.1 = 0, R3.0 = main (2D)
    "F83FA2F811A1D372"     # 0008  R2.0 = stack (3F), R1.0 = interrupt (11), P = 3, LDXA
    "70227822" "52C4C4C4"  # 0010  RET, DEC 2, SAV, DEC 2, STR 2, three NOPs
    "F800B0F800A080E2"     # 0018  R0 = 0000, GLO 0, SEX 2
    "E220A0E220A0E220"     # 0020  three gaps that put R0 back
    "A03C1E300FE2693F"     # 0028  BN1 1E, BR 0F, main: SEX 2, INP 1 (TV on), BN4 2F
    "2F6CA437333F356C"     # 0030  wait for IN, INP 4, PLO 4, wait release, wait IN, INP 4
    "5414303300000000"     # 0038  STR 4, INC 4, BR 33, stack area
)
assert len(table1) == 0x40

# Table II, the spaceship program (0040 to 00FF)
rows = """
0040 00 00 0C 00 00 00 00 00
0048 00 00 00 00 00 00 00 00
0050 7B DE DB DE 00 00 00 00
0058 4A 50 DA 52 00 00 00 00
0060 42 5E AB D0 00 00 00 00
0068 4A 42 8A 52 00 00 00 00
0070 7B DE 8A 5E 00 00 00 00
0078 00 00 00 00 00 00 00 00
0080 00 00 00 00 00 00 07 E0
0088 00 00 00 00 FF FF FF FF
0090 00 06 00 01 00 00 00 01
0098 00 7F E0 01 00 00 00 02
00A0 7F C0 3F E0 FC FF FF FE
00A8 40 0F 00 10 04 80 00 00
00B0 7F C0 3F E0 04 80 00 00
00B8 00 3F D0 40 04 80 00 00
00C0 00 0F 08 20 04 80 7A 1E
00C8 00 00 07 90 04 80 42 10
00D0 00 00 18 7F FC F0 72 1C
00D8 00 00 30 00 00 10 42 10
00E0 00 00 73 FC 00 10 7B D0
00E8 00 30 30 00 3F F0 00 00
00F0 00 00 18 0F C0 00 00 00
00F8 00 30 07 F0 00 00 00 00
"""
img = bytearray(table1)
for line in rows.strip().splitlines():
    parts = line.split()
    assert int(parts[0], 16) == len(img), (parts[0], len(img))
    img += bytes(int(p, 16) for p in parts[1:])
assert len(img) == 256
open(OUT, "wb").write(img)
print("wrote", OUT, len(img), "bytes")
