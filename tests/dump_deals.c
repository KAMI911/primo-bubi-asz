/* Writes random deals with the engine's verdict for tests/oracle.py to re-check with independent code.
   Line: game (0 hold'em, 1 omaha) | 4 seats' cards | board | 4 scores (hex)   cards are like "AS" (rank A23456789TJQK, suit SHDC); folded seats do not exist here
   Besides plain random deals there are skewed decks (two suits, low ranks, few ranks) so flushes, straights, wheels,
   full houses, quads and straight flushes are common. Usage: dump_deals out.txt [deals per game and mode] */
#include <stdio.h>
#include <stdlib.h>
#include "../src/poker.c"
#include "../src/ai.c"
#include "../src/cards.c"

static void put(FILE *f, card_t c) { fprintf(f, " %c%c", "A23456789TJQK"[RANK(c)], "SHDC"[SUIT(c)]); }

int main(int argc, char **argv) {
    FILE *f = fopen(argc > 1 ? argv[1] : "build/deals.txt", "w");
    long per = argc > 2 ? atol(argv[2]) : 3000, i;
    uint8_t g, mode, k, s, pool[52], np, j;
    Table t;
    if (!f) return 1;
    rng_seed(99);
    for (g = 0; g < NPGAMES; g++) for (mode = 0; mode < 5; mode++) for (i = 0; i < per; i++) {
        np = 0;
        for (k = 0; k < 52; k++) {                                   /* the cards the deal may use */
            uint8_t r = RANK(k % 13), su = (uint8_t)(k / 13);
            (void)r;
            if (mode == 1 && su > 1) continue;                      /* two suits: flushes everywhere */
            if (mode == 2 && (k % 13) > 6) continue;                /* ace..7: straights, wheels */
            if (mode == 3 && (k % 13) > 4 && (k % 13) < 12) continue;   /* A-5 and K: few ranks, pairs, trips, quads, full houses */
            if (mode == 4 && ((k % 13) > 5 || su > 1)) continue;    /* A-6 in two suits: straight flushes */
            pool[np++] = MKCARD(k % 13, su);
        }
        if (np < 18) continue;
        for (j = (uint8_t)(np - 1); j > 0; j--) { uint8_t r2 = (uint8_t)rng_below((uint16_t)(j + 1)), x = pool[j]; pool[j] = pool[r2]; pool[r2] = x; }
        t.game = g; t.nb = 0;
        { uint8_t hole = g == PG_HOLDEM ? 2 : 4, p = 0;
          if (np < (uint8_t)(hole * NSEAT + 5)) continue;
          for (s = 0; s < NSEAT; s++) { t.n[s] = hole; for (k = 0; k < hole; k++) t.c[s][k] = pool[p++]; }
          for (k = 0; k < 5; k++) t.board[t.nb++] = pool[p++]; }
        fprintf(f, "%u |", (unsigned)g);
        for (s = 0; s < NSEAT; s++) { for (k = 0; k < t.n[s]; k++) put(f, t.c[s][k]); fprintf(f, " |"); }
        for (k = 0; k < t.nb; k++) put(f, t.board[k]);
        fprintf(f, " |");
        for (s = 0; s < NSEAT; s++) fprintf(f, " %lx", (unsigned long)eval_seat(&t, s));
        fprintf(f, "\n");
    }
    fclose(f);
    return 0;
}
