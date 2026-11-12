#!/usr/bin/env python3
"""emu_pack.py prog.bin script.bin out.bin: the program image (loaded at $4400) padded up to $DCC0, then the key script."""
import sys
prog = open(sys.argv[1], "rb").read(); script = open(sys.argv[2], "rb").read()
off = 0xDCC0 - 0x4400
if len(prog) > off: sys.exit("emu_pack: the program is larger than $DCC0")
if len(script) > 0x0980:                                   # $DCC0 + $980 = $E640: below the deepest stack ($E7F0 - 423)
    sys.exit("emu_pack: the script is too long")
open(sys.argv[3], "wb").write(prog + bytes(off - len(prog)) + script)
