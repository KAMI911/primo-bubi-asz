#!/usr/bin/env python3
"""Convert the host preview PBM files to PNG (x3 scale) so they can be looked at. Needs Pillow."""
import sys, glob
from PIL import Image
for f in sorted(glob.glob(sys.argv[1] + "/*.pbm")):
    d = open(f, "rb").read()
    hdr, rest = d.split(b"\n", 2)[0:2], d.split(b"\n", 2)[2]
    im = Image.frombytes("1", (256, 192), rest).convert("L").resize((768, 576), Image.NEAREST)
    im.save(f[:-4] + ".png")
    print(f[:-4] + ".png")
