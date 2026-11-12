#!/usr/bin/env python3
"""Hungarian tape builds: the sources hold the accented capitals as UTF-8 in the Hungarian texts; the Primo font has no
such letters, so this turns them into the character codes 0x60..0x68 (96..104), extra glyphs of the Hungarian font (src/font.c):
0x60 A-acute  0x61 E-acute  0x62 I-acute  0x63 O-acute  0x64 O-umlaut  0x65 O-double-acute  0x66 U-acute  0x67 U-umlaut  0x68 U-double-acute.
Usage: accents.py in.c out.c"""
import sys
CODES = "ÁÉÍÓÖŐÚÜŰ"
src = open(sys.argv[1], encoding="utf-8").read()
for i, ch in enumerate(CODES):
    src = src.replace(ch, "\\%03o" % (0x60 + i))
bad = [c for c in src if ord(c) > 127]
if bad:
    sys.exit("accents.py: unsupported character(s) %r in %s" % ("".join(sorted(set(bad))), sys.argv[1]))
open(sys.argv[2], "w", encoding="utf-8").write(src)
