#include "cards.h"

static uint16_t rng_state = 1;

void rng_seed(uint16_t s) { rng_state = s ? s : 0xACE1u; }  /* xorshift must never be 0 */

uint16_t rng_next(void) {
    uint16_t x = rng_state;
    x ^= (uint16_t)(x << 7);
    x ^= (uint16_t)(x >> 9);
    x ^= (uint16_t)(x << 8);
    rng_state = x;
    return x;
}

uint16_t rng_below(uint16_t n) {
    uint16_t rem, limit, r;
    if (n <= 1) return 0;
    rem = (uint16_t)((0xFFFFu % n) + 1u);        /* 65536 mod n */
    if (rem == n) rem = 0;
    limit = (uint16_t)(0u - rem);                /* 65536 - rem; wraps to 0 = accept everything */
    do { r = rng_next(); } while (limit != 0 && r >= limit);
    return (uint16_t)(r % n);
}

uint8_t deck_build(card_t *out, uint8_t decks, uint8_t suits) {
    uint8_t d, s, r, k = 0;
    for (d = 0; d < decks; d++)
        for (s = 0; s < 4; s++)
            for (r = 0; r < 13; r++)
                out[k++] = MKCARD(r, suits >= 4 ? s : (suits == 2 ? (s & 1) ? 1 : 0 : 0));
    return k;
}

void deck_shuffle(card_t *d, uint8_t n) {
    uint8_t i, j;
    card_t t;
    for (i = n; i > 1; i--) {
        j = (uint8_t)rng_below(i);
        t = d[(uint8_t)(i - 1)]; d[(uint8_t)(i - 1)] = d[j]; d[j] = t;
    }
}
