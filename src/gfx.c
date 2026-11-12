#include <stdint.h>
#include <string.h>
#include "gfx.h"
#include "hw.h"

extern const uint8_t font_glyphs[];                        /* src/font.c: ASCII 32..95 */
#define FONT font_glyphs

static uint8_t inv;                         /* 0x00 normal, 0xFF inverted output */

/* ---- colour model ------------------------------------------------------------------------------------
   Every 4x4 pixel cell has a nibble; with the pixel bit it selects one of 32 palette colours. Pixel 1 is always
   white, pixel 0 is the colour of the nibble: 0 table green, 1 black (black symbols on white), 2 red, 3 blue (card back). */
#define NIB_TABLE 0
#define NIB_BLACK 1
#define NIB_RED   2
#define NIB_BACK  3
#ifdef TARGET_BW
/* monochrome build: no colour code at all */
#define cram ((uint8_t *)0)
void gfx_init(void) { }
void gfx_set_table(uint8_t t) { (void)t; }
void gfx_set_back_colour(uint8_t c) { (void)c; }
void gfx_title_colours(uint8_t on) { (void)on; }
static void paint(uint8_t x4, uint8_t y, uint8_t w4, uint8_t h, uint8_t nib) { (void)x4; (void)y; (void)w4; (void)h; (void)nib; }
#else
extern const uint16_t palette_color[16];     /* the game's palette plus the colour pairs of the commentators' portraits (src/faces.c) */
#define palette palette_color
static uint8_t *cram;                        /* colour RAM, 0 on a monochrome model */
static uint8_t table_col = 0x0C;             /* RRRGGGBB of the table: green, burgundy or black */

static uint8_t title_pal;                    /* the title screen's palette: colours 4..7 are the blue, yellow, green and red ink of the credit line */
static uint8_t back_col = 0x02;              /* RRRGGGBB of the card backs (the palette's colour 3): blue, red or green */

static void apply_palette(void) {            /* the palette with the chosen card back colour in it */
    uint16_t p[16];
    uint8_t i;
    for (i = 0; i < 16; i++) p[i] = palette[i];
    p[1] = (uint16_t)((p[1] & 0x00FF) | ((uint16_t)back_col << 8));
    if (title_pal) { p[2] = p[3] = 0; p[10] = 0xFC0B; p[11] = 0xE01C; }     /* black behind, the ink is the pixel's colour */
    hw_color_setup(p, table_col);
}

void gfx_set_table(uint8_t t) {
    static const uint8_t col[3] = {0x0C, 0x60, 0x00};
    table_col = col[t % 3];
    if (cram) apply_palette();
}

void gfx_title_colours(uint8_t on) {
    title_pal = on;
    if (cram) apply_palette();
}

void gfx_set_back_colour(uint8_t c) {
    static const uint8_t col[3] = {0x02, 0xA0, 0x18};
    back_col = col[c % 3];
    if (cram) apply_palette();
}

void gfx_init(void) {
    uint16_t n;
    cram = hw_color_origin();
    if (!cram) return;
    apply_palette();
    for (n = 0; n < 49 * 32; n++) cram[n] = 0;
}

static void paint(uint8_t x4, uint8_t y, uint8_t w4, uint8_t h, uint8_t nib);   /* below, after fill_row() */
#endif

void gfx_set_invert(uint8_t on) { inv = on ? 0xFF : 0x00; }

/* ---- 8x8 suit glyphs: spade, heart, diamond, club (filled), heart + diamond outlines (red suits, so they
   stay distinguishable on a monochrome screen) ------------------------------------------------------ */
static const uint8_t suit_glyph[6][8] = {
    {0x18, 0x3C, 0x7E, 0xFF, 0xFF, 0x66, 0x18, 0x3C},
    {0x66, 0xFF, 0xFF, 0xFF, 0x7E, 0x3C, 0x18, 0x00},
    {0x18, 0x3C, 0x7E, 0xFF, 0xFF, 0x7E, 0x3C, 0x18},
    {0x18, 0x3C, 0x3C, 0xE7, 0xFF, 0xFF, 0x18, 0x3C},
    {0x66, 0x99, 0x81, 0x81, 0x42, 0x24, 0x18, 0x00},
    {0x18, 0x24, 0x42, 0x81, 0x81, 0x42, 0x24, 0x18},
};
static const uint8_t *suit_bits(uint8_t suit) {
    if (suit == 1) return suit_glyph[4];     /* hearts: outline */
    if (suit == 2) return suit_glyph[5];     /* diamonds: outline */
    return suit_glyph[suit == 0 ? 0 : 3];
}

/* 8x8 tiles for the card backs */
static const uint8_t back_tile[NBACKS][8] = {
    {0x88, 0x44, 0x22, 0x11, 0x88, 0x44, 0x22, 0x11},   /* diagonal stripes */
    {0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F},   /* big checks */
    {0x00, 0x66, 0x66, 0x00, 0x00, 0x66, 0x66, 0x00},   /* dots */
    {0xFF, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81},   /* grid */
    {0x18, 0x24, 0x42, 0x81, 0x81, 0x42, 0x24, 0x18},   /* diamonds */
    {0xFF, 0x08, 0x08, 0x08, 0xFF, 0x80, 0x80, 0x80},   /* bricks */
};

/* ---- low level --------------------------------------------------------------------------------- */
static uint8_t *row_ptr(uint8_t y) { return hw_vram() + (uint16_t)y * SCR_BYTES; }

void gfx_clear(void) {
    memset(hw_vram(), inv, (uint16_t)SCR_H * SCR_BYTES);                   /* (memset is a block move on the Z80) */
    if (cram) memset(cram, 0, 49 * 32);
    hw_poll();
}

/* sets w4 (>= 1) nibbles of one line from nibble x4 on: one block fill of all the bytes they touch, then the other half of an
   odd first and an odd last byte is put back; v holds the same nibble twice (0x00, 0xFF or n * 17) */
static void fill_row(uint8_t *row, uint8_t x4, uint8_t w4, uint8_t v) {
    uint8_t f = (uint8_t)(x4 >> 1), l = (uint8_t)((uint8_t)(x4 + w4 - 1) >> 1), a = row[f], b = row[l];
    memset(row + f, v, (uint8_t)(l - f + 1));
    if (x4 & 1) row[f] = (uint8_t)((a & 0xF0) | (v & 0x0F));
    if ((uint8_t)(x4 + w4) & 1) row[l] = (uint8_t)((v & 0xF0) | (b & 0x0F));
}

/* one card is composed in RAM (3 bytes per line), then blitted with nibble accuracy */
static uint8_t cbuf[CARD_H * 3];

static void blit(uint8_t x4, uint8_t y, const uint8_t *buf, uint8_t rows) {
    uint8_t *base = row_ptr(y) + (x4 >> 1);
    uint8_t r;
    uint8_t odd = (uint8_t)(x4 & 1);
    for (r = 0; r < rows && (uint8_t)(y + r) < SCR_H; r++, buf += 3, base += SCR_BYTES) {
        if (!odd) {
            base[0] = buf[0] ^ inv; base[1] = buf[1] ^ inv; base[2] = buf[2] ^ inv;
        } else {
            base[0] = (uint8_t)((base[0] & 0xF0) | ((buf[0] ^ inv) >> 4));
            base[1] = (uint8_t)(((buf[0] ^ inv) << 4) | ((buf[1] ^ inv) >> 4));
            base[2] = (uint8_t)(((buf[1] ^ inv) << 4) | ((buf[2] ^ inv) >> 4));
            base[3] = (uint8_t)(((buf[2] ^ inv) << 4) | (base[3] & 0x0F));
        }
    }
}

#ifndef TARGET_BW
static void paint(uint8_t x4, uint8_t y, uint8_t w4, uint8_t h, uint8_t nib) {   /* set the cells covering a rectangle */
    uint8_t r, r1, *p;
    if (!cram || !h || !w4) return;
    r1 = (uint8_t)((uint8_t)(y + h - 1) >> 2);
    if (r1 > 48) r1 = 48;
    for (r = (uint8_t)(y >> 2), p = cram + (uint16_t)r * 32; r <= r1; r++, p += 32) fill_row(p, x4, w4, (uint8_t)(nib * 17));
    hw_poll();
}
#endif

void gfx_fill(uint8_t x4, uint8_t y, uint8_t w4, uint8_t h, uint8_t white) {
    uint8_t v = (uint8_t)((white ? 0xFF : 0x00) ^ inv), rows = h, *p;
    if (y < SCR_H && w4) {
        if (rows > (uint8_t)(SCR_H - y)) rows = (uint8_t)(SCR_H - y);
        for (p = row_ptr(y); rows--; p += SCR_BYTES) fill_row(p, x4, w4, v);
    }
    if (!white) paint(x4, y, w4, h, NIB_TABLE);
    hw_poll();
}

/* ---- text --------------------------------------------------------------------------------------- */
#ifdef LANG_HU
#define GLYPH_MAX 104                                      /* 96..104: the accented capitals (src/font.c) */
#else
#define GLYPH_MAX 95
#endif
static const uint8_t *glyph(char ch) {
    uint8_t c = (uint8_t)ch;
    if (c < 32 || c > GLYPH_MAX) c = '?';
    return &FONT[(uint16_t)(c - 32) * 8];
}

/* The text loop is the hot spot of every screen, so on the Z80 it is assembly: draws the 8-line characters of the string a_s from the
   screen address a_p (32 bytes per line, each byte xor a_m) up to the end of the line or of the string; a character the font does not
   have is drawn as '?'. The video RAM must start at a multiple of 32 (it does: $E800, $E700). The host builds use the C loop. */
#ifdef __SDCC
static uint8_t *a_p;
static const char *a_s;
static uint8_t a_m;
static const uint8_t a_gmax = GLYPH_MAX;
static void text8(void) __naked {
    __asm
    ld hl,(_a_p)
    ld de,(_a_s)
    ld a,(_a_m)
    ld c,a
t8_char:
    ld a,(de)
    or a
    ret z
    inc de
    push de
    push hl
    ld b,a
    cp 32
    jr c,t8_q
    ld a,(_a_gmax)
    cp b
    ld a,b
    jr nc,t8_ok
t8_q:
    ld a,63
t8_ok:
    sub 32
    ld l,a
    ld h,0
    add hl,hl
    add hl,hl
    add hl,hl
    ld de,_font_glyphs
    add hl,de
    ex de,hl
    pop hl
    push hl
    ld b,8
t8_row:
    ld a,(de)
    xor c
    ld (hl),a
    inc de
    ld a,l
    add a,32
    ld l,a
    jr nc,t8_nc
    inc h
t8_nc:
    djnz t8_row
    pop hl
    inc hl
    ld a,l
    and 31
    jr nz,t8_more
    pop de
    ret
t8_more:
    and 3
    jr nz,t8_next
    push bc
    push hl
    call _hw_poll
    pop hl
    pop bc
t8_next:
    pop de
    jr t8_char
    __endasm;
}
#endif

static void draw_text(uint8_t xb, uint8_t y, const char *s, uint8_t reverse) {
    uint8_t m = (uint8_t)((reverse ? 0xFF : 0x00) ^ inv), n = 0;
    if (cram) { while (s[n] && (uint8_t)(xb + n) < SCR_BYTES) n++; paint((uint8_t)(xb * 2), y, (uint8_t)(n * 2), 8, reverse ? NIB_BLACK : NIB_TABLE); }
#ifdef __SDCC
    if (y > SCR_H - 8 || xb >= SCR_BYTES) return;                  /* (no text is ever drawn below the last full line) */
    a_p = row_ptr(y) + xb; a_s = s; a_m = m;
    text8();
#else
    {
        uint8_t k, *p;
        const uint8_t *g;
        for (; *s && xb < SCR_BYTES; s++, xb++) {
            g = glyph(*s);
            p = row_ptr(y) + xb;
            for (k = 0; k < 8 && (uint8_t)(y + k) < SCR_H; k++, p += SCR_BYTES) *p = g[k] ^ m;
            if (!(xb & 3)) hw_poll();                             /* every 4th character is often enough (see hw.h) */
        }
    }
#endif
}

void gfx_text(uint8_t xb, uint8_t y, const char *s, uint8_t reverse) { draw_text(xb, y, s, reverse); }

void gfx_text_c(uint8_t y, const char *s, uint8_t reverse) {
    uint8_t n = 0;
    while (s[n]) n++;
    draw_text((uint8_t)(n >= SCR_BYTES ? 0 : (SCR_BYTES - n) / 2), y, s, reverse);
}

void gfx_credit(uint8_t y, const char *by) {              /* centred "<by> KAMI & AI": K blue, A yellow, M green, I red (colour model) */
    char s[2] = {0, 0};
    uint8_t n = 0, x, i;
    while (by[n]) n++;
    x = (uint8_t)((SCR_BYTES - n - 10) / 2);
    draw_text(x, y, by, 0);
    x = (uint8_t)(x + n + 1);
    for (i = 0; i < 4; i++) {
        s[0] = "KAMI"[i];
        draw_text((uint8_t)(x + i), y, s, (uint8_t)(i & 1));
        paint((uint8_t)((x + i) * 2), y, 2, 8, (uint8_t)(4 + i));
    }
    draw_text((uint8_t)(x + 4), y, " & AI", 0);
}

static uint16_t spread(uint8_t b) {                      /* 8 bits -> 16 bits, every bit doubled */
    uint16_t r = 0; uint8_t i;
    for (i = 0; i < 8; i++) if (b & (uint8_t)(0x80 >> i)) r |= (uint16_t)(3u << (14 - 2 * i));
    return r;
}

void gfx_text2(uint8_t xb, uint8_t y, const char *s) {
    uint8_t k;
    uint16_t w;
    const uint8_t *g;
    uint8_t *p;
    for (; *s && xb + 1 < SCR_BYTES; s++, xb = (uint8_t)(xb + 2)) {
        g = glyph(*s);
        for (k = 0; k < 8; k++) {
            w = spread(g[k]);
            if ((uint8_t)(y + 2 * k + 1) >= SCR_H) break;
            p = row_ptr((uint8_t)(y + 2 * k)) + xb;
            p[0] = (uint8_t)(w >> 8) ^ inv; p[1] = (uint8_t)w ^ inv;
            p += SCR_BYTES;
            p[0] = (uint8_t)(w >> 8) ^ inv; p[1] = (uint8_t)w ^ inv;
        }
        hw_poll();
    }
}

uint8_t gfx_u16(char *out, uint16_t v) {
    char t[5]; uint8_t n = 0, i;
    do { t[n++] = (char)('0' + (v % 10)); v = (uint16_t)(v / 10); } while (v);
    for (i = 0; i < n; i++) out[i] = t[n - 1 - i];
    return n;
}

void gfx_hline(uint8_t y, uint8_t x4, uint8_t w4) { gfx_fill(x4, y, w4, 1, 1); }

void gfx_plot(uint8_t x, uint8_t y, uint8_t white) {
    uint8_t *p, m = (uint8_t)(0x80 >> (x & 7));
    if (y >= SCR_H) return;
    p = row_ptr(y) + (x >> 3);
    if (white ^ (inv != 0)) *p |= m; else *p &= (uint8_t)~m;
}

void gfx_bubble(uint8_t x4, uint8_t y, uint8_t w4, uint8_t h, uint8_t tail_x) {
    uint8_t i, x = (uint8_t)(x4 * 4), w = (uint8_t)(w4 * 4);
    gfx_fill(x4, y, w4, h, 1);
    paint(x4, y, w4, h, NIB_BLACK);
    for (i = 0; i < 3; i++) {                                   /* rounded corners */
        gfx_plot((uint8_t)(x + i), y, 0); gfx_plot((uint8_t)(x + w - 1 - i), y, 0);
        gfx_plot((uint8_t)(x + i), (uint8_t)(y + h - 1), 0); gfx_plot((uint8_t)(x + w - 1 - i), (uint8_t)(y + h - 1), 0);
    }
    gfx_plot(x, (uint8_t)(y + 1), 0); gfx_plot((uint8_t)(x + w - 1), (uint8_t)(y + 1), 0);
    gfx_plot(x, (uint8_t)(y + h - 2), 0); gfx_plot((uint8_t)(x + w - 1), (uint8_t)(y + h - 2), 0);
    for (i = 0; i < 6; i++) {                                   /* tail: a small triangle below the box */
        uint8_t k;
        for (k = i; k < (uint8_t)(12 - i); k++) gfx_plot((uint8_t)(tail_x + k), (uint8_t)(y + h + i), 1);
    }
    hw_poll();
}

/* ---- the commentators' portraits: 48x48 bitmaps made by tools/gen_faces.py from a photo (src/faces.c) --------------------- */
extern const uint8_t face_bits[2][288];
#ifndef TARGET_BW
extern const uint8_t face_nib[2][72];
#endif

void gfx_face(uint8_t x4, uint8_t y, uint8_t who) {
    const uint8_t *f = face_bits[who & 1];
    uint8_t r, k;
    for (r = 0; r < 48; r++) {
        uint8_t *p = row_ptr((uint8_t)(y + r)) + (x4 >> 1);
        for (k = 0; k < 6; k++) p[k] = (uint8_t)(*f++ ^ inv);
    }
#ifdef TARGET_BW
    paint(x4, y, 12, 48, NIB_BLACK);
#else
    if (cram) for (r = 0; r < 144; r++) {                          /* colour model: a colour pair per 4x4 cell */
        uint8_t n = face_nib[who & 1][r >> 1];
        paint((uint8_t)(x4 + r % 12), (uint8_t)(y + 4 * (r / 12)), 1, 4, (r & 1) ? (uint8_t)(n & 15) : (uint8_t)(n >> 4));
    }
#endif
    hw_poll();
}

/* ---- portraits ---------------------------------------------------------------------------------- */
static void clip_x(int16_t x4, uint8_t w4, uint8_t *a, uint8_t *n) {       /* visible cells of [x4, x4 + w4) */
    int16_t lo = x4 < 0 ? 0 : x4, hi = (int16_t)(x4 + w4) > 64 ? 64 : (int16_t)(x4 + w4);
    *a = (uint8_t)lo; *n = hi > lo ? (uint8_t)(hi - lo) : 0;
}

void gfx_erase(int16_t x4, uint8_t y, uint8_t w4, uint8_t h) {
    uint8_t a, n;
    clip_x(x4, w4, &a, &n);
    if (n) gfx_fill(a, y, n, h, 0);
}


/* one 4-pixel column pc of the panel, drawn in screen cell column sx (0..63) */
/* ---- cards -------------------------------------------------------------------------------------- */
static void put_glyph(uint8_t col, uint8_t row, const uint8_t *g) {       /* black symbol on the white card */
    uint8_t k;
    for (k = 0; k < 8; k++) cbuf[(uint8_t)(row + k) * 3 + col] &= (uint8_t)~g[k];
}

static void compose_face(card_t c) {
    static const char rank_chr[13] = {'A', '2', '3', '4', '5', '6', '7', '8', '9', '1', 'J', 'Q', 'K'};
    uint8_t i, k, col = 0, r = RANK(c), s = SUIT(c);
    const uint8_t *sg = suit_bits(s);
    uint16_t w;
    for (i = 0; i < CARD_H * 3; i++) cbuf[i] = 0xFF;
    if (!cram) { cbuf[0] = 0x7F; cbuf[2] = 0xFE; cbuf[(CARD_H - 1) * 3] = 0x7F; cbuf[(CARD_H - 1) * 3 + 2] = 0xFE; }   /* rounded corners (monochrome only: the cut pixels would show as coloured dots) */
    put_glyph(col++, 2, glyph(rank_chr[r]));
    if (r == 9) put_glyph(col++, 2, glyph('0'));
    put_glyph(col, 2, sg);
    for (k = 0; k < 8; k++) {                                              /* big pip: the suit glyph at double size */
        w = spread(sg[k]);                                                  /* the pip sits one pixel left of the card's right edge */
        cbuf[(uint8_t)(12 + 2 * k) * 3]     &= (uint8_t)~(w >> 15);
        cbuf[(uint8_t)(12 + 2 * k) * 3 + 1] &= (uint8_t)~(w >> 7);
        cbuf[(uint8_t)(12 + 2 * k) * 3 + 2] &= (uint8_t)~(w << 1);
        cbuf[(uint8_t)(13 + 2 * k) * 3]     &= (uint8_t)~(w >> 15);
        cbuf[(uint8_t)(13 + 2 * k) * 3 + 1] &= (uint8_t)~(w >> 7);
        cbuf[(uint8_t)(13 + 2 * k) * 3 + 2] &= (uint8_t)~(w << 1);
    }
}

static void compose_back(uint8_t style) {
    uint8_t i, y, t;
    const uint8_t *tile = back_tile[style % NBACKS];
    for (i = 0; i < CARD_H * 3; i++) cbuf[i] = 0;
    cbuf[0] = cram ? 0xFF : 0x7F; cbuf[1] = 0xFF; cbuf[2] = cram ? 0xFF : 0xFE;
    cbuf[(CARD_H - 1) * 3] = cram ? 0xFF : 0x7F; cbuf[(CARD_H - 1) * 3 + 1] = 0xFF; cbuf[(CARD_H - 1) * 3 + 2] = cram ? 0xFF : 0xFE;
    for (y = 1; y < CARD_H - 1; y++) { cbuf[y * 3] = 0x80; cbuf[y * 3 + 2] = 0x01; }
    for (y = 3; y < CARD_H - 3; y++) {                                     /* pattern inside a black margin */
        t = tile[y & 7];
        cbuf[y * 3]     |= (uint8_t)(t & 0x1F);
        cbuf[y * 3 + 1] |= t;
        cbuf[y * 3 + 2] |= (uint8_t)(t & 0xF8);
    }
}

void gfx_card(uint8_t x4, uint8_t y, card_t c, uint8_t back) {
    if (IS_UP(c)) compose_face(c); else compose_back(back);
    blit(x4, y, cbuf, CARD_H);
    paint(x4, y, CARD_W4, CARD_H, IS_UP(c) ? (IS_RED(c) ? NIB_RED : NIB_BLACK) : NIB_BACK);
    hw_poll();
}

void gfx_slot(uint8_t x4, uint8_t y) {
    uint8_t i;
    for (i = 0; i < CARD_H * 3; i++) cbuf[i] = 0;
    for (i = 0; i < CARD_H; i += 2) { cbuf[i * 3] = 0x80; cbuf[i * 3 + 2] = 0x01; }
    cbuf[0] = 0xAA; cbuf[1] = 0xAA; cbuf[2] = 0xAA;
    cbuf[(CARD_H - 1) * 3] = 0x55; cbuf[(CARD_H - 1) * 3 + 1] = 0x55; cbuf[(CARD_H - 1) * 3 + 2] = 0x55;
    blit(x4, y, cbuf, CARD_H);
    hw_poll();
}

/* ---- XOR overlays (cursor, selection) ----------------------------------------------------------- */
static void xor_span(uint8_t y, uint8_t xpx, uint8_t wpx) {              /* invert wpx pixels starting at xpx on line y */
    uint8_t *p, first, last, m;
    if (y >= SCR_H || !wpx) return;
    p = row_ptr(y);
    first = (uint8_t)(xpx >> 3);
    last = (uint8_t)((uint8_t)(xpx + wpx - 1) >> 3);
    for (; first <= last; first++) {
        m = 0xFF;
        if (first == (uint8_t)(xpx >> 3)) m &= (uint8_t)(0xFF >> (xpx & 7));
        if (first == last) m &= (uint8_t)(0xFF << (7 - ((uint8_t)(xpx + wpx - 1) & 7)));
        p[first] ^= m;
    }
}

void gfx_xor_rect(uint8_t x4, uint8_t y, uint8_t w4, uint8_t h) {
    uint8_t r;
    for (r = 0; r < h; r++) xor_span((uint8_t)(y + r), (uint8_t)(x4 * 4), (uint8_t)(w4 * 4));
    hw_poll();
}

void gfx_xor_frame(uint8_t x4, uint8_t y, uint8_t w4, uint8_t h) {
    uint8_t r, x = (uint8_t)(x4 * 4), w = (uint8_t)(w4 * 4);
    if (h < 4 || w < 4) return;
    for (r = 0; r < 2; r++) {
        xor_span((uint8_t)(y + r), x, w);
        xor_span((uint8_t)(y + h - 1 - r), x, w);
    }
    for (r = 2; r < h - 2; r++) {
        xor_span((uint8_t)(y + r), x, 2);
        xor_span((uint8_t)(y + r), (uint8_t)(x + w - 2), 2);
    }
    hw_poll();
}
