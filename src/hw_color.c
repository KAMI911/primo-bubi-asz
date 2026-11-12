#include "hw.h"

/* Colour Primo (C / Pro), as verified against the Raktaros sources and its emulator screenshot:
     $E001        control: 0x81 = colour on, palette bank 1
     $E020/40/60  three palette banks of 16 words; each word holds two 8-bit RRRGGGBB colours, stored inverted
     $E080        colour RAM: one nibble per 4x4 pixel cell (high nibble = left cell), 32 bytes per 4 pixel lines
   Colour of a pixel = palette byte number ((pixel << 4) | nibble): word index>>1, low byte for even, high for odd.
   Offsets are taken from the colour RAM pointer so a host build can point it at a plain buffer. */
void hw_color_setup(const uint16_t *pal, uint8_t table) {
    uint8_t *c = hw_color_ram();
    uint8_t *b1, *b2, *b3, *ctl;
    uint8_t i, lo, hi;
    uint16_t v;
    if (!c) return;
    b1 = c - 0x60; b2 = c - 0x40; b3 = c - 0x20; ctl = c - 0x7F;
    for (i = 0; i < 16; i++) {
        v = (uint16_t)~pal[i];
        lo = (uint8_t)v; hi = (uint8_t)(v >> 8);
        b1[(uint8_t)(2 * i)] = lo; b1[(uint8_t)(2 * i + 1)] = hi;
        b2[(uint8_t)(2 * i)] = lo; b2[(uint8_t)(2 * i + 1)] = hi;
        b3[(uint8_t)(2 * i)] = lo; b3[(uint8_t)(2 * i + 1)] = hi;
    }
    b1[0] = b2[0] = b3[0] = (uint8_t)~table;                 /* colour 0 = the table */
    *ctl = 0x81;
}
