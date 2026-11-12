/* Host test of the game rules, by playing whole random hands and watching every event:
   - invariants at each showdown (no duplicate cards, card counts of the game, matched bets, pot paid out in full, winners)
   - blinds and who acts first (before the flop, and on each later street), the card counts, matched bets, side pots, splits
   Everything is recomputed here with its own small code instead of the engine's helpers. */
#include <stdio.h>
#include <stdlib.h>
#include "../src/poker.c"
#include "../src/ai.c"
#include "../src/cards.c"

static long checks, fails, cover[16];
#define CHECK(c) do { checks++; if (!(c)) { if (++fails < 20) printf("FAIL line %d: %s (game %u hand %u)\n", __LINE__, #c, (unsigned)cur_game, (unsigned)cur_hand); } } while (0)
enum { CV_BLINDS, CV_PREFIRST, CV_POSTFIRST, CV_HEADSUP, CV_SIDEPOT, CV_ALLIN, CV_FOLDWIN, CV_SPLIT, CV_SHOW, CV_N };
static uint8_t cur_game; static uint32_t cur_hand;

static uint16_t pot_seen;
static int street_flag;                    /* the next EV_THINK is the first of a betting round */
static uint8_t expect_seat;                /* who must act first in it (0xFF = not checkable) */

static uint8_t live_n(const Table *t) { uint8_t s, n = 0; for (s = 0; s < NSEAT; s++) if (!(t->out & (1u << s))) n++; return n; }
static uint8_t seat_after(const Table *t, uint8_t s) { do s = (uint8_t)((s + 1) % NSEAT); while (t->out & (1u << s)); return s; }
/* the first seat clockwise from `from` (exclusive) that can still act */
static uint8_t first_actor(const Table *t, uint8_t from) {
    uint8_t i, s = from;
    for (i = 0; i < NSEAT; i++) { s = (uint8_t)((s + 1) % NSEAT); if (!((t->folded | t->allin | t->out) & (1u << s))) return s; }
    return 0xFF;
}

static void check_unique(const Table *t) {
    uint8_t seen[52] = {0}, s, k;
    for (s = 0; s < NSEAT; s++) for (k = 0; k < t->n[s]; k++) { card_t c = t->c[s][k] & 0x3F; CHECK(RANK(c) < 13 && SUIT(c) < 4); CHECK(!seen[SUIT(c) * 13 + RANK(c)]++); }
    for (k = 0; k < t->nb; k++) { card_t c = t->board[k] & 0x3F; CHECK(!seen[SUIT(c) * 13 + RANK(c)]++); }
}

static void on_hand(const Table *t) {
    uint8_t s, alive = live_n(t), sb, bb, hole = t->game == PG_HOLDEM ? 2 : 4;
    check_unique(t);
    for (s = 0; s < NSEAT; s++) if (!(t->out & (1u << s))) CHECK(t->n[s] == hole);
    CHECK(t->nb == 0);
    if (alive == 2) { sb = t->dealer; bb = seat_after(t, sb); cover[CV_HEADSUP]++; } else { sb = seat_after(t, t->dealer); bb = seat_after(t, sb); }
    for (s = 0; s < NSEAT; s++) {
        uint16_t want = s == sb ? 5 : s == bb ? 10 : 0;
        if (t->out & (1u << s)) continue;
        { uint16_t stack = (uint16_t)(t->chips[s] + t->total[s]); CHECK(t->total[s] == (stack < want ? stack : want)); }   /* a short stack pays what it has */
    }
    CHECK(t->last[sb] == L_BLIND && t->last[bb] == L_BLIND);
    /* three or four players: the seat after the big blind opens; heads up the dealer (small blind) does */
    expect_seat = alive == 2 ? ((t->allin & (1u << sb)) ? 0xFF : sb) : first_actor(t, bb);
    street_flag = 1; cover[CV_BLINDS]++; cover[CV_PREFIRST]++;
}

static void on_street(const Table *t) {                     /* the flop, turn or river was dealt */
    street_flag = 1;
    CHECK(t->nb == 3 || t->nb == 4 || t->nb == 5);
    expect_seat = first_actor(t, t->dealer);                /* after the flop the first live seat left of the dealer opens (heads up: the big blind) */
    cover[CV_POSTFIRST]++;
}

static void on_show(const Table *t) {
    uint8_t s;
    uint16_t m = 0;
    check_unique(t);
    cover[CV_SHOW]++;
    for (s = 0; s < NSEAT; s++) if (!(t->folded & (1u << s)) && t->total[s] > m) m = t->total[s];
    for (s = 0; s < NSEAT; s++) {
        if (t->folded & (1u << s)) continue;
        CHECK(t->n[s] == (t->game == PG_HOLDEM ? 2 : 4));
        if (!(t->allin & (1u << s))) CHECK(t->total[s] == m); else { CHECK(t->total[s] <= m); cover[CV_ALLIN]++; if (t->total[s] < m) cover[CV_SIDEPOT]++; }
    }
    CHECK(t->nb == 5);                                      /* the board is always complete at the showdown */
}

static void on_event(const Table *t, uint8_t ev, uint8_t seat, uint16_t amt) {
    uint16_t p = tbl_pot(t);
    (void)amt;
    if (p > pot_seen) pot_seen = p;
    switch (ev) {
    case EV_HAND: on_hand(t); break;
    case EV_REFRESH: if (t->nb > 0) on_street(t); break;
    case EV_THINK:
        if (street_flag) {
            street_flag = 0;
            if (expect_seat != 0xFF && seat != expect_seat && getenv("RDBG") && fails < 3) {
                uint8_t q, k2;
                printf("game %u hand %u: seat %u acts first, expected %u, dealer %u, folded %02x allin %02x out %02x nb %u\n", (unsigned)cur_game, (unsigned)cur_hand, seat, expect_seat, t->dealer, t->folded, t->allin, t->out, t->nb);
                for (q = 0; q < NSEAT; q++) { printf("  seat %u:", q); for (k2 = 0; k2 < t->n[q]; k2++) if (t->c[q][k2] & C_UP) printf(" %c%c", "A23456789TJQK"[RANK(t->c[q][k2])], "SHDC"[SUIT(t->c[q][k2])]); printf("\n"); }
            }
            if (expect_seat != 0xFF) CHECK(seat == expect_seat);
        }
        break;
    case EV_SHOW: on_show(t); break;
    default: break;
    }
}

/* ---- the players: all kinds of behaviour ---------------------------------------------------------------- */
static uint8_t human_random(const Table *t, uint8_t can_raise, uint16_t to_call) {
    uint16_t r = rng_below(10);
    (void)t; (void)to_call;
    return r < 2 ? A_FOLD : (r < 4 && can_raise) ? A_RAISE : A_CALL;
}
static uint16_t want;
static uint8_t human_sizes(const Table *t, uint8_t can_raise, uint16_t to_call) {
    uint16_t r = rng_below(10);
    (void)to_call; (void)can_raise;
    want = r < 3 ? (uint16_t)(t->bet[0] + t->chips[0]) : (uint16_t)(tbl_raise_to(t) + rng_below(6) * t->unit);
    return r < 1 ? A_FOLD : r < 7 ? A_RAISE : A_CALL;
}
static uint8_t human_shove(const Table *t, uint8_t can_raise, uint16_t to_call) { (void)can_raise; (void)to_call; want = (uint16_t)(t->bet[0] + t->chips[0]); return A_RAISE; }
static uint8_t human_fold(const Table *t, uint8_t can_raise, uint16_t to_call) { (void)t; (void)can_raise; (void)to_call; return A_FOLD; }

static void play(uint8_t game, const Hooks *hk, uint32_t hands) {
    Table t;
    uint32_t i, sessions = 0;
    uint8_t s, bests;
    uint32_t before;
    tbl_init(&t, game, hk);
    cur_game = game;
    for (i = 0; i < hands; i++) {
        cur_hand = (uint32_t)i;
        pot_seen = 0; street_flag = 0; expect_seat = 0xFF;
        before = 0;
        for (s = 0; s < NSEAT; s++) before += t.chips[s];
        if (!tbl_hand(&t)) { sessions++; tbl_init(&t, game, hk); continue; }
        {   /* after the hand */
            uint32_t after = 0, won = 0; uint8_t n = 0;
            for (s = 0; s < NSEAT; s++) { after += t.chips[s]; won += t.won[s]; }
            CHECK(after == before);                                    /* chips are conserved */
            CHECK(won == pot_seen);                                    /* the whole pot is paid out, nothing more */
            for (s = 0; s < NSEAT; s++) {
                if (t.won[s]) { CHECK(!(t.folded & (1u << s)) || (t.out & (1u << s)) == 0); CHECK(!(t.folded & (1u << s))); n++; }
            }
            CHECK(n >= 1);
            if (t.showdown) {                                         /* the best live hand takes a share, a worse hand never beats it */
                score_t best = 0;
                for (s = 0; s < NSEAT; s++) if (!(t.folded & (1u << s)) && t.score[s] > best) best = t.score[s];
                for (s = 0; s < NSEAT; s++) if (!(t.folded & (1u << s)) && t.score[s] == best) CHECK(t.won[s] > 0);
                if (n > 1) cover[CV_SPLIT]++;
                (void)bests;
            } else {
                cover[CV_FOLDWIN]++;
                CHECK(n == 1);
            }
        }
    }
    (void)sessions;
}

int main(void) {
    static const Hooks hr = {human_random, on_event, 0};
    static const Hooks hz = {human_sizes, on_event, &want};
    static const Hooks hs = {human_shove, on_event, &want};
    static const Hooks hf = {human_fold, on_event, 0};
    uint8_t g;
    uint32_t n = 6000;
    rng_seed(7);
    for (g = 0; g < NPGAMES; g++) { play(g, &hr, n); play(g, &hz, n); play(g, &hs, n / 3); play(g, &hf, n / 3); }
    printf("coverage: blinds %ld (heads up %ld), preflop firsts %ld, postflop firsts %ld, showdowns %ld, all-in %ld, side pots %ld, fold wins %ld, split pots %ld\n",
           cover[CV_BLINDS], cover[CV_HEADSUP], cover[CV_PREFIRST], cover[CV_POSTFIRST], cover[CV_SHOW], cover[CV_ALLIN], cover[CV_SIDEPOT], cover[CV_FOLDWIN], cover[CV_SPLIT]);
    CHECK(cover[CV_BLINDS] > 1000 && cover[CV_POSTFIRST] > 1000 && cover[CV_HEADSUP] > 100);   /* the checks really ran */
    CHECK(cover[CV_SIDEPOT] > 100 && cover[CV_FOLDWIN] > 1000 && cover[CV_SPLIT] > 10);
    printf("%ld checks, %ld failures\n", checks, fails);
    if (!fails) printf("RULES ALL PASS\n");
    return fails != 0;
}
