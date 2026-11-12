#!/usr/bin/env python3
"""check_stack.py emu_out.txt map...: the stack depth measured in the emulator run (monochrome build) plus a margin for the colour
build's extra drawing code must fit between each tape's RAM top (end of its variables) and the stack start ($DFF0 colour, $E7F0 mono)."""
import re, sys
out = open(sys.argv[1]).read()
m = re.search(r"^STACK (\d+)", out, re.M)
if not m: sys.exit("check_stack: no STACK line in the emulator output (did the run reach the end of the script?)")
depth = int(m.group(1)); MARGIN = 48; bad = 0
print("deepest stack use in the emulator run: %d bytes (+%d margin)" % (depth, MARGIN))
for f in sys.argv[2:]:
    t = re.search(r"^__tail\s+= \$([0-9A-Fa-f]+)", open(f).read(), re.M)
    top = int(t.group(1), 16); start = 0xDFF0 if "color" in f else 0xE7F0
    room = start - top
    ok = depth + MARGIN <= room; bad |= not ok
    print("  %-34s RAM top %04X, room for the stack %4d bytes: %s" % (f.split("/")[-1], top, room, "ok" if ok else "TOO LITTLE"))
sys.exit(1 if bad else 0)
