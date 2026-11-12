#ifndef CARDS_H
#define CARDS_H
#include <stdint.h>

/* A card is one byte: bits 0-3 rank (0 = Ace .. 12 = King), bits 4-5 suit, bit 7 = face up. */
typedef uint8_t card_t;

#define C_UP        0x80u
#define C_RANKMASK  0x0Fu
#define RANK(c)     ((uint8_t)((c) & C_RANKMASK))
#define SUIT(c)     ((uint8_t)(((c) >> 4) & 3u))
#define MKCARD(r,s) ((card_t)((r) | ((s) << 4)))
#define IS_UP(c)    (((c) & C_UP) != 0)
/* suits: 0 spade, 1 heart, 2 diamond, 3 club -> hearts and diamonds are red */
#define IS_RED(c)   (SUIT(c) == 1 || SUIT(c) == 2)

/* 16-bit xorshift PRNG; the deal number is the seed, so a deal can be replayed. */
void     rng_seed(uint16_t s);
uint16_t rng_next(void);
/* uniform value in 0..n-1 (rejection sampling, no modulo bias); n >= 1 */
uint16_t rng_below(uint16_t n);

/* Fills out[] with decks*52 cards (face down). suits: 4 = normal; 1 or 2 = Spider style
   (only that many suits, repeated to still give decks*52 cards). Returns the count. */
uint8_t  deck_build(card_t *out, uint8_t decks, uint8_t suits);
/* Fisher-Yates shuffle using the PRNG. */
void     deck_shuffle(card_t *d, uint8_t n);

#endif
