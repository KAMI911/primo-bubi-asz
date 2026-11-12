/* The drawing routines against a naive reference model: random clears, fills and texts (also with the Hungarian accent codes, clipped
   at the screen edges, in inverse mode) must leave exactly the same pixels - and in the colour build the same colour cells - as a
   pixel-by-pixel model. On the Z80 build this runs the assembly text loop. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/gfx.h"
#include "../src/hw.h"
#include "../src/gfx.c"

#ifdef __SDCC
static uint8_t vram_raw[6144 + 32];
#define VRAM ((uint8_t *)(((uint16_t)vram_raw + 31) & ~31u))
#else
static uint8_t vram_raw[6144] __attribute__((aligned(32)));
#define VRAM vram_raw
#endif
static uint8_t cramb[50 * 32];
uint8_t *hw_vram(void) { return VRAM; }
#ifdef TARGET_BW
uint8_t *hw_color_origin(void) { return 0; }
uint8_t *hw_color_ram(void) { return 0; }
uint8_t hw_is_color(void) { return 0; }
#define HAVE_COLOR 0
#else
uint8_t *hw_color_origin(void) { return cramb; }
uint8_t *hw_color_ram(void) { return cramb; }
uint8_t hw_is_color(void) { return 1; }
#define HAVE_COLOR 1
#endif
void hw_color_setup(const uint16_t *pal16, uint8_t table) { (void)pal16; (void)table; }
uint16_t polls;
void hw_poll(void) { polls++; }
uint16_t hw_frames(void) { return 0; }
void hw_wait_frame(void) { }
const uint8_t pk_tok[1][2] = {{0, 0}};

static uint8_t mv[6144], mc[50 * 32];                       /* the model: pixels and colour cells */
static uint8_t minv;
static uint16_t rs = 0xACE1;
static uint16_t rnd(uint16_t n) { rs ^= (uint16_t)(rs << 7); rs ^= (uint16_t)(rs >> 9); rs ^= (uint16_t)(rs << 8); return (uint16_t)(rs % n); }   /* 16-bit xorshift: cheap on the Z80 */
static long checks, fails;

static void bitset(uint8_t x, uint8_t y, int b) { uint8_t m = (uint8_t)(0x80 >> (x & 7)); if (b) mv[y * 32 + (x >> 3)] |= m; else mv[y * 32 + (x >> 3)] &= (uint8_t)~m; }
static void cellset(uint8_t n, uint8_t r, uint8_t v) { uint8_t *b = &mc[r * 32 + (n >> 1)]; *b = (n & 1) ? (uint8_t)((*b & 0xF0) | v) : (uint8_t)((*b & 0x0F) | (v << 4)); }
static void mpaint(uint8_t x4, uint8_t y, uint8_t w4, uint8_t h, uint8_t nib) {
    uint8_t r, r1, n;
    if (!HAVE_COLOR || !h || !w4) return;
    r1 = (uint8_t)((uint8_t)(y + h - 1) >> 2); if (r1 > 48) r1 = 48;
    for (r = (uint8_t)(y >> 2); r <= r1; r++) for (n = x4; n < (uint8_t)(x4 + w4); n++) cellset(n, r, nib);
}
static void mfill(uint8_t x4, uint8_t y, uint8_t w4, uint8_t h, uint8_t white) {
    unsigned r, x;
    for (r = 0; r < h && y + r < SCR_H; r++) for (x = (unsigned)x4 * 4; x < (unsigned)(x4 + w4) * 4 && x < 256; x++) bitset((uint8_t)x, (uint8_t)(y + r), (white ? 1 : 0) ^ (minv ? 1 : 0));
    if (!white) mpaint(x4, y, w4, h, NIB_TABLE);
}
static void mglyph(uint8_t c, uint8_t g[8]) {                /* the reference character shapes, written out separately from gfx.c */
    uint8_t k;
#ifdef LANG_HU
    static const char base[] = "AEIOOOUUU";
    static const uint8_t mark[9] = {0x08, 0x08, 0x08, 0x08, 0x24, 0x14, 0x08, 0x24, 0x14};
    if (c >= 96 && c <= 104) { for (k = 0; k < 8; k++) g[k] = font_glyphs[(base[c - 96] - 32) * 8 + k]; g[0] = mark[c - 96]; return; }   /* the accent glyph = capital + mark in the top line */
    if (c > 104) c = '?';
#else
    if (c > 95) c = '?';
#endif
    if (c < 32) c = '?';
    for (k = 0; k < 8; k++) g[k] = font_glyphs[(c - 32) * 8 + k];
}
static void mtext(uint8_t xb, uint8_t y, const char *s, uint8_t rev) {
    uint8_t n = 0, g[8], k, col, m;
    while (s[n] && (uint8_t)(xb + n) < SCR_BYTES) n++;
    if (HAVE_COLOR) mpaint((uint8_t)(xb * 2), y, (uint8_t)(n * 2), 8, rev ? NIB_BLACK : NIB_TABLE);
    for (col = 0; col < n; col++) {
        mglyph((uint8_t)s[col], g);
        for (k = 0; k < 8 && y + k < SCR_H; k++) {
            m = (uint8_t)((rev ? 0xFF : 0) ^ (minv ? 0xFF : 0));
            mv[(y + k) * 32 + xb + col] = g[k] ^ m;
        }
    }
}

static void compare(const char *what, unsigned i) {
    unsigned k; int bad = 0;
    checks++;
    if (memcmp(VRAM, mv, 6144)) { bad = 1; for (k = 0; k < 6144 && VRAM[k] == mv[k]; k++) ; printf("FAIL %s #%u: pixel byte %u (x %u y %u) is %02x, model %02x\n", what, i, k, (k % 32) * 8, k / 32, VRAM[k], mv[k]); }
    if (HAVE_COLOR && memcmp(cramb, mc, 49 * 32)) { bad = 1; for (k = 0; k < 49 * 32 && cramb[k] == mc[k]; k++) ; printf("FAIL %s #%u: colour byte %u is %02x, model %02x\n", what, i, k, cramb[k], mc[k]); }
    if (bad) { fails++; if (fails > 5) { printf("too many failures\n"); exit(1); } }
}

static void scramble(void) {                                   /* random screen and colour RAM (one random byte per 8 bytes, rotated) */
    unsigned k; uint8_t v = 0;
    for (k = 0; k < 6144; k++) { if (!(k & 7)) v = (uint8_t)rnd(256); mv[k] = VRAM[k] = (uint8_t)(v = (uint8_t)((v << 3) | (v >> 5)) ^ (uint8_t)k); }
    for (k = 0; k < 50 * 32; k++) { if (!(k & 7)) v = (uint8_t)rnd(256); mc[k] = cramb[k] = (uint8_t)(v = (uint8_t)((v << 5) | (v >> 3)) ^ (uint8_t)k); }
}

int main(void) {
    unsigned i, k;
    char s[48];
    gfx_init();
#ifdef __SDCC
#define ROUNDS 20                                            /* the emulated Z80 is slow: fewer rounds */
#else
#define ROUNDS 400
#endif
    for (i = 0; i < ROUNDS; i++) {
        minv = (uint8_t)(rnd(4) == 0); gfx_set_invert(minv);
        if (i % 10 == 0) { scramble(); gfx_clear(); memset(mv, minv ? 0xFF : 0, 6144); if (HAVE_COLOR) memset(mc, 0, 49 * 32); compare("clear", i); }
        scramble();
        {   /* fills, also over the edges: odd/even start and width, widths 0..64, clipped at the bottom */
            unsigned f;
            for (f = 0; f < 6; f++) {
                uint8_t x4 = (uint8_t)rnd(64), w4 = (uint8_t)rnd(65 - x4 > 40 ? 41 : 65 - x4), y = (uint8_t)rnd(192), h = (uint8_t)rnd(60), wh = (uint8_t)rnd(2);
                gfx_fill(x4, y, w4, h, wh); mfill(x4, y, w4, h, wh);
            }
            compare("fill", i);
        }
        {   /* texts: all characters, accent codes, runs up to and over the end of the line, rows up to the last line and over it */
            unsigned t;
            for (t = 0; t < 8; t++) {
                #ifdef __SDCC
                uint8_t len = (uint8_t)rnd(40), xb = (uint8_t)rnd(32), y = (uint8_t)rnd(185), rev = (uint8_t)rnd(2);      /* the Z80 build draws no text below the last full line */
#else
                uint8_t len = (uint8_t)rnd(40), xb = (uint8_t)rnd(32), y = (uint8_t)(t < 6 ? rnd(185) : 180 + rnd(12)), rev = (uint8_t)rnd(2);
#endif
                for (k = 0; k < len; k++) { uint16_t r = rnd(100); s[k] = (char)(r < 8 ? 1 + rnd(31) : r < 30 ? 90 + rnd(25) : r < 35 ? 105 + rnd(23) : 32 + rnd(64));   /* controls, the accent codes, beyond them, ordinary */ }
                s[len] = 0;
                gfx_text(xb, y, s, rev); mtext(xb, y, s, rev);
            }
            compare("text", i);
        }
    }
    printf("%ld comparisons, %ld failures\n", checks, fails);
    if (!fails) printf("GFX ALL PASS\n");
    return fails != 0;
}
