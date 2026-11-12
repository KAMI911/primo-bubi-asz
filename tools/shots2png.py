#!/usr/bin/env python3
"""Turns screen dumps into PNG (needs Pillow).
   shots2png.py out.txt dir      the SHOT/SHOTC hex dumps printed by the Z80 emulator run (make emu)
   shots2png.py --col file.col   a colour dump written by the host tools
Colour model (verified against the Raktaros emulator screenshot): colour RAM holds one nibble per 4x4 pixel cell
(high nibble = left cell); palette byte number = (pixel << 4) | nibble; palette words are stored inverted, low
byte = even entry, high byte = odd entry; a colour byte is RRRGGGBB."""
import re, sys, os
from PIL import Image

def rgb(b): return (((b >> 5) & 7) * 36, ((b >> 2) & 7) * 36, (b & 3) * 85)

def render_color(pix, cram, pal):
    words = [(~(pal[2*i] | (pal[2*i+1] << 8))) & 0xFFFF for i in range(16)]
    im = Image.new("RGB", (256, 192))
    px = im.load()
    for y in range(192):
        for x in range(256):
            bit = (pix[y*32 + (x >> 3)] >> (7 - (x & 7))) & 1
            cb = cram[(y >> 2) * 32 + (x >> 3)]
            nib = (cb >> 4) if (x & 7) < 4 else (cb & 15)
            idx = (bit << 4) | nib
            w = words[idx >> 1]
            px[x, y] = rgb((w >> 8) if (idx & 1) else (w & 255))
    return im.resize((768, 576), Image.NEAREST)

def parse_hex(lines): return bytes.fromhex("".join(lines))

def main():
    if sys.argv[1] == "--col":
        t = open(sys.argv[2]).read().split("\n")
        body = [l for l in t[1:] if re.fullmatch(r"[0-9A-F]{64}", l)]
        pix, cram, pal = parse_hex(body[:192]), parse_hex(body[192:242]), parse_hex(body[242:243])
        out = sys.argv[2][:-4] + ".png"; render_color(pix, cram, pal).save(out); print(out); return
    txt = open(sys.argv[1]).read(); d = sys.argv[2]; os.makedirs(d, exist_ok=True)
    for m in re.finditer(r"SHOT(C?) (\d+)\n((?:[0-9A-F]{64}\n)+)END", txt):
        rows = m.group(3).split("\n")[:-1]; name = os.path.join(d, "emu%02d.png" % int(m.group(2)))
        if m.group(1):
            render_color(parse_hex(rows[:192]), parse_hex(rows[192:242]), parse_hex(rows[242:243])).save(name)
        else:
            Image.frombytes("1", (256, 192), parse_hex(rows[:192])).convert("L").resize((768, 576), Image.NEAREST).save(name)
        print(name)
main()
