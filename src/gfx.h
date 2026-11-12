#ifndef GFX_H
#define GFX_H
#include <stdint.h>
#include "cards.h"

#define SCR_BYTES   32          /* bytes per pixel line (256 px) */
#define SCR_H       192
#define CARD_W4     6           /* card width in 4-pixel units (24 px) */
#define CARD_H      32
#define STATUS_Y    184         /* the status line occupies the last 8 lines */
#define NBACKS      6

void gfx_init(void);                              /* colour model: load the palette and reset the colour RAM */
void gfx_set_table(uint8_t t);                   /* colour model: table colour 0 green, 1 burgundy, 2 black (no effect on a monochrome model) */
void gfx_set_invert(uint8_t on);                 /* 1 = swap black and white (if a model shows the opposite polarity) */
void gfx_clear(void);
void gfx_fill(uint8_t x4, uint8_t y, uint8_t w4, uint8_t h, uint8_t white);
void gfx_title_colours(uint8_t on);                                  /* colour model: the title screen's palette (credit line colours) on or off */
void gfx_credit(uint8_t y, const char *by);                          /* centred "<by> KAMI & AI": K blue, A yellow, M green, I red */
void gfx_set_back_colour(uint8_t c);                                 /* colour model: card backs blue (0), red (1) or green (2) */
void gfx_card(uint8_t x4, uint8_t y, card_t c, uint8_t back);   /* face up if C_UP is set, else card back #back */
void gfx_slot(uint8_t x4, uint8_t y);                            /* empty-pile outline */
void gfx_xor_frame(uint8_t x4, uint8_t y, uint8_t w4, uint8_t h);/* 2-px inverting frame (cursor); draw twice to erase */
void gfx_xor_rect(uint8_t x4, uint8_t y, uint8_t w4, uint8_t h); /* invert a rectangle (selected cards) */
/* text: x in bytes (8 px) 0..31, y in pixels; 8x8 font */
void gfx_text(uint8_t xb, uint8_t y, const char *s, uint8_t reverse);
void gfx_text_c(uint8_t y, const char *s, uint8_t reverse);       /* centred */
void gfx_text2(uint8_t xb, uint8_t y, const char *s);             /* double size, 16x16 per character */
uint8_t gfx_u16(char *out, uint16_t v);                           /* decimal into out (no terminator); returns length */
void gfx_hline(uint8_t y, uint8_t x4, uint8_t w4);
void gfx_plot(uint8_t x, uint8_t y, uint8_t white);
/* comic speech bubble: white rounded box (x4 even) with a tail pointing down at pixel column tail_x */
void gfx_bubble(uint8_t x4, uint8_t y, uint8_t w4, uint8_t h, uint8_t tail_x);

/* portrait of a commentator, 48x48 pixels at x4 (even, up to 50): who 0 Korda Gyorgy, 1 Klarika */
void gfx_face(uint8_t x4, uint8_t y, uint8_t who);

void gfx_erase(int16_t x4, uint8_t y, uint8_t w4, uint8_t h);            /* table-coloured rectangle, clipped to the screen */

#endif
