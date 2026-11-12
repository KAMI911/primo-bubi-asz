/* Host (and Z80) tests of the poker engine: hand ranking, side pots, whole hands for both games, chip conservation. */
#include <stdio.h>
#include <stdlib.h>
#include "../src/poker.c"
#include "../src/ai.c"
#include "../src/cards.c"

#ifdef QUICK                      /* the emulated Z80 run: same checks, far fewer random hands */
#define SEVEN_N 60
#define SIM_HANDS 6
#else
#define SEVEN_N 3000
#define SIM_HANDS 300
#endif
static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

/* cards from text such as "AS KH 9D": rank A 2-9 T J Q K, suit S H D C */
static uint8_t parse(const char *s, card_t *out) {
    uint8_t n = 0;
    while (*s) {
        const char *r = "A23456789TJQK";
        uint8_t i, su;
        while (*s == ' ') s++;
        if (!*s) break;
        for (i = 0; r[i] != s[0]; i++) ;
        su = s[1] == 'S' ? 0 : s[1] == 'H' ? 1 : s[1] == 'D' ? 2 : 3;
        out[n++] = MKCARD(i, su);
        s += 2;
    }
    return n;
}
static score_t ev(const char *s) { card_t c[7]; uint8_t n = parse(s, c); return eval_n(c, n); }

static void test_ranking(void) {
    /* ascending list of hands: each must beat the previous one */
    static const char *const order[] = {
        "2C 3D 5H 7S 9C", "2C 3D 5H 7S TC", "2C 3D 5H 8S TC", "2C 4D 5H 8S TC", "3C 4D 5H 8S TC", "2C 3D 5H JS KC", "2C 3D 5H JS AC",
        "2C 2D 5H 7S 9C", "2C 2D 3H 7S TC", "2C 2D 5H 7S TC", "2C 2D 6H 7S TC", "3C 3D 4H 7S 9C", "AC AD 5H 7S 9C", "AC AD KH 7S 9C", "AC AD KH QS 9C",
        "2C 2D 3H 3S 9C", "2C 2D 3H 3S AC", "2C 2D 4H 4S 3C", "KC KD QH QS 3C", "KC KD QH QS 4C", "AC AD 2H 2S 3C", "AC AD KH KS 2C",
        "2C 2D 2H 7S 9C", "2C 2D 2H 8S 9C", "2C 2D 2H 8S TC", "3C 3D 3H 4S 5C", "AC AD AH KS QC",
        "AH 2D 3H 4S 5C", "2H 3D 4H 5S 6C", "5H 6D 7H 8S 9C", "TH JD QH KS AC",
        "2C 4C 6C 8C TC", "2C 4C 6C 8C QC", "3C 4C 6C 8C QC", "2D 9D JD KD AD",
        "2C 2D 2H 3S 3C", "3C 3D 3H 2S 2C", "AC AD AH KS KC",
        "2C 2D 2H 2S 3C", "2C 2D 2H 2S KC", "3C 3D 3H 3S 2C", "AC AD AH AS KC",
        "AH 2H 3H 4H 5H", "2H 3H 4H 5H 6H", "9H TH JH QH KH", "TH JH QH KH AH"};
    uint8_t n = (uint8_t)(sizeof order / sizeof order[0]), i;
    for (i = 1; i < n; i++) {
        if (!(ev(order[i]) > ev(order[i - 1]))) printf("order: %s should beat %s\n", order[i], order[i - 1]);
        CHECK(ev(order[i]) > ev(order[i - 1]));
    }
    CHECK(SCORE_CAT(ev("AH 2D 3H 4S 5C")) == H_STRAIGHT);
    CHECK(ev("AH 2D 3H 4S 5C") < ev("2H 3D 4H 5S 6C"));                 /* the wheel is the lowest straight */
    CHECK(ev("2C 2D 5H 7S 9C") == ev("2H 2S 5D 7C 9S"));                /* suits do not matter */
    CHECK(SCORE_CAT(ev("AH KH QH JH TH")) == H_SFLUSH);
    /* seven cards */
    CHECK(SCORE_CAT(ev("AH KH QH JH TH 2C 2D")) == H_SFLUSH);
    CHECK(SCORE_CAT(ev("2H 2D 2C 3H 3D 3C 9S")) == H_FULL);            /* two triples: full house */
    CHECK(ev("2H 2D 2C 3H 3D 3C 9S") == ev("3H 3D 3C 2H 2D"));
    CHECK(SCORE_CAT(ev("2H 2D 3C 3H 5D 5C 9S")) == H_TWOPAIR);          /* three pairs: the two highest, best kicker */
    CHECK(ev("2H 2D 3C 3H 5D 5C 9S") == ev("5C 5D 3C 3H 9S"));
    CHECK(ev("2H 2D 3C 3H 5D 5C AS") == ev("5C 5D 3C 3H AS"));
    CHECK(SCORE_CAT(ev("2H 3D 4C 5H 6D 8C 9S")) == H_STRAIGHT);
    CHECK(SCORE_CAT(ev("2H 3H 4H 5H 9H AC AD")) == H_FLUSH);
    CHECK(SCORE_CAT(ev("2H 3H 4H 5H 6D KH 9H")) == H_FLUSH);            /* flush beats the straight in the same cards */
    CHECK(SCORE_CAT(ev("2H 3H 4H 5H 6H 7H 8H")) == H_SFLUSH);
    CHECK(ev("2H 3H 4H 5H 6H 7H 8H") == ev("4H 5H 6H 7H 8H"));          /* the highest straight flush of the seven */
    CHECK(SCORE_CAT(ev("AS AD AC AH KS KD KC")) == H_QUADS);
    CHECK(SCORE_CAT(ev("KS KD")) == H_PAIR);                            /* fewer than five cards (the flop-less seat) */
    CHECK(ev("KS KD 2C") > ev("QS QD AC"));
    CHECK(ev("AS 5D") > ev("KS QD"));
}

/* brute force: the best of all 21 five-card subsets must equal eval_n of the seven */
static void test_seven_vs_brute(void) {
    uint32_t iter;
    card_t d[52], h[7], f[5];
    uint8_t i, a, b, c, e, g, k;
    rng_seed(77);
    for (iter = 0; iter < SEVEN_N; iter++) {
        score_t best = 0;
        deck_build(d, 1, 4); deck_shuffle(d, 52);
        for (i = 0; i < 7; i++) h[i] = d[i];
        for (a = 0; a < 7; a++) for (b = (uint8_t)(a + 1); b < 7; b++) {
            k = 0;
            for (c = 0; c < 7; c++) if (c != a && c != b) f[k++] = h[c];
            if (eval_n(f, 5) > best) best = eval_n(f, 5);
        }
        (void)e; (void)g;
        CHECK(best == eval_n(h, 7));
    }
}

#ifndef QUICK
/* category frequencies of random five-card hands against the textbook values */
static void test_frequencies(void) {
    uint32_t cnt[9] = {0}, n = 200000, i;
    card_t d[52];
    rng_seed(4242);
    for (i = 0; i < n; i++) { deck_build(d, 1, 4); deck_shuffle(d, 52); cnt[SCORE_CAT(eval_n(d, 5))]++; }
    printf("5-card hand frequencies (per 100000): ");
    for (i = 0; i < 9; i++) printf("%u ", (unsigned)(cnt[i] * 100000ull / n));
    printf("\n");
    CHECK(cnt[H_HIGH] * 1000ull / n > 480 && cnt[H_HIGH] * 1000ull / n < 520);      /* 50.1 % */
    CHECK(cnt[H_PAIR] * 1000ull / n > 405 && cnt[H_PAIR] * 1000ull / n < 440);      /* 42.3 % */
    CHECK(cnt[H_TWOPAIR] * 1000ull / n > 40 && cnt[H_TWOPAIR] * 1000ull / n < 55);  /* 4.75 % */
    CHECK(cnt[H_TRIPS] * 1000ull / n > 17 && cnt[H_TRIPS] * 1000ull / n < 26);      /* 2.11 % */
}

#endif

static void test_omaha(void) {
    card_t h[4], b[5];
    parse("AH KH 2C 3D", h); parse("QH JH TH 9C 4D", b);
    CHECK(SCORE_CAT(eval_omaha(h, 4, b, 5)) == H_SFLUSH);               /* AH KH + QH JH TH */
    parse("AH 2H 3C 3D", h); parse("KH QH JH TH 4D", b);
    CHECK(SCORE_CAT(eval_omaha(h, 4, b, 5)) == H_STRAIGHT || SCORE_CAT(eval_omaha(h, 4, b, 5)) == H_FLUSH);
    parse("AH 2C 3C 4D", h); parse("KH QH JH TH 4S", b);                /* one heart in hand: the four-flush board is no flush */
    CHECK(SCORE_CAT(eval_omaha(h, 4, b, 5)) == H_PAIR);                /* no straight or flush without two hole cards: only the pair of fours */
}

/* ---- side pots ---------------------------------------------------------------------------------------- */
static void test_pots(void) {
    Table t;
    uint8_t s;
    for (s = 0; s < NSEAT; s++) { t.chips[s] = 0; t.total[s] = 0; t.score[s] = 0; t.won[s] = 0; }
    t.dealer = 3;
    /* seat 0 all in for 50 with the best hand, 1 and 2 put in 100, seat 3 folded after 20 */
    t.folded = 8; t.total[0] = 50; t.total[1] = 100; t.total[2] = 100; t.total[3] = 20;
    t.score[0] = 900; t.score[1] = 500; t.score[2] = 700;
    award(&t);
    CHECK((uint16_t)(t.won[0] + t.won[1] + t.won[2] + t.won[3]) == 270);
    /* main pot: 4 contributions capped at 50 = 50+50+50+20 = 170 to seat 0; side pot 100 total: 50+50 -> seat 2 */
    CHECK(t.won[0] == 170);
    CHECK(t.won[2] == 100);
    CHECK(t.won[1] == 0);
    /* an uncalled excess goes back to its owner: seat 1 bets 100, seat 2 only 40 (all in), seat 2 wins */
    for (s = 0; s < NSEAT; s++) { t.chips[s] = 0; t.total[s] = 0; t.won[s] = 0; t.score[s] = 0; }
    t.folded = 9; t.total[1] = 100; t.total[2] = 40; t.score[1] = 100; t.score[2] = 200;
    award(&t);
    CHECK(t.won[2] == 80 && t.won[1] == 60);
    /* a tie splits the pot, the odd chip goes to the first winner after the dealer */
    for (s = 0; s < NSEAT; s++) { t.chips[s] = 0; t.total[s] = 0; t.won[s] = 0; t.score[s] = 0; }
    t.folded = 0; t.dealer = 0; t.total[0] = 11; t.total[1] = 11; t.total[2] = 11; t.total[3] = 10;
    t.score[0] = 5; t.score[1] = 9; t.score[2] = 9; t.score[3] = 1;
    award(&t);
    CHECK(t.won[1] + t.won[2] == 43 && t.won[1] == 22 && t.won[2] == 21);
}


/* ---- the computer players' betting: good hands are bet and raised, air is given up ------------------------------------------ */
static Table at;
/* seat 1 holds `hole`, the board is `board`, `live` seats are in the hand; each of them has put in `in` chips; seat 1 has the style `style` */
static void setup_ai(const char *hole, const char *board, uint8_t live, uint16_t in, uint8_t style, uint8_t game) {
    uint8_t s;
    at.game = game; at.nb = parse(board, at.board); at.n[1] = parse(hole, at.c[1]);
    at.folded = 0; at.out = 0; at.allin = 0; at.style[1] = style; at.raises = 0; at.cur = 0;
    for (s = 0; s < NSEAT; s++) { at.total[s] = in; at.bet[s] = 0; at.chips[s] = 400; if (s >= live) at.folded |= (uint8_t)(1u << s); }
}
/* how many percent of 400 decisions are a raise / a fold */
static void ai_rates(uint16_t to_call, uint8_t raises, uint8_t *raise_pct, uint8_t *fold_pct) {
    uint16_t i, r = 0, f = 0;
    at.raises = raises; at.cur = to_call;
    for (i = 0; i < 400; i++) { uint8_t a = ai_act(&at, 1, 1, to_call); if (a == A_RAISE) r++; else if (a == A_FOLD) f++; }
    *raise_pct = (uint8_t)(r / 4); *fold_pct = (uint8_t)(f / 4);
}
static void test_ai_aggression(void) {
    uint8_t st, r, f;
    rng_seed(31337);
    for (st = 0; st < 4; st++) {
        /* two pair on the flop, everybody checked to it: it must be bet (the calling station style less eagerly) */
        setup_ai("KH 7D", "KD 7C 2S", 4, 20, st, PG_HOLDEM); ai_rates(0, 0, &r, &f);
        CHECK(r >= (st == S_STATION ? 70 : 95));
        /* the same heads up and three handed */
        setup_ai("KH 7D", "KD 7C 2S", 2, 20, st, PG_HOLDEM); ai_rates(0, 0, &r, &f); CHECK(r >= 90);
        setup_ai("KH 7D", "KD 7C 2S", 3, 20, st, PG_HOLDEM); ai_rates(0, 0, &r, &f); CHECK(r >= 90);
        /* three of a kind facing a bet: raise it, never fold */
        setup_ai("KH KS", "KD 7C 2S", 4, 20, st, PG_HOLDEM); ai_rates(10, 1, &r, &f); CHECK(r >= (st == S_STATION ? 50 : 80) && f == 0);
        /* two pair facing a bet and a raise: still never folds */
        setup_ai("KH 7D", "KD 7C 2S", 4, 20, st, PG_HOLDEM); ai_rates(10, 2, &r, &f); CHECK(f == 0);
        /* nothing at all facing a bet into a small pot: mostly gives up (a station and a loose player hang on longer) */
        setup_ai("9H 4D", "KD 7C 2S", 4, 10, st, PG_HOLDEM); ai_rates(10, 1, &r, &f); CHECK(r <= 8 && f >= (st == S_STATION ? 0 : 25));
    }
    /* the top pair with the best kicker is bet by the aggressive style at a full table, and by everybody heads up */
    setup_ai("AH KD", "KS 7C 2S", 4, 20, S_AGGRO, PG_HOLDEM); ai_rates(0, 0, &r, &f); CHECK(r >= 30);
    setup_ai("AH KD", "KS 7C 2S", 2, 20, S_TIGHT, PG_HOLDEM); ai_rates(0, 0, &r, &f); CHECK(r >= 50);
    setup_ai("AH KD", "KS 7C 2S", 4, 20, S_TIGHT, PG_HOLDEM); ai_rates(0, 0, &r, &f); CHECK(r >= 10);
    /* it does not bet nothing: a low card high board with nothing in the hand */
    setup_ai("9H 4D", "KD 7C 2S", 4, 20, S_TIGHT, PG_HOLDEM); ai_rates(0, 0, &r, &f); CHECK(r <= 10);
    /* before the flop: aces are raised, trash is not */
    setup_ai("AH AD", "", 4, 0, S_TIGHT, PG_HOLDEM); at.total[0] = 5; at.total[1] = 10; ai_rates(10, 1, &r, &f); CHECK(r >= 90);
    setup_ai("7H 2D", "", 4, 0, S_TIGHT, PG_HOLDEM); at.total[0] = 5; at.total[1] = 10; ai_rates(10, 1, &r, &f); CHECK(r <= 2);
    /* a re-raise needs more than a bet: aces re-raise a raise, a middling pair only calls */
    setup_ai("AH AD", "", 4, 0, S_TIGHT, PG_HOLDEM); at.total[0] = 5; at.total[1] = 10; at.total[2] = 20; ai_rates(10, 2, &r, &f); CHECK(r >= 70);
    setup_ai("8H 8D", "", 4, 0, S_TIGHT, PG_HOLDEM); at.total[0] = 5; at.total[1] = 10; at.total[2] = 20; ai_rates(10, 2, &r, &f); CHECK(r <= 20 && f <= 30);
    /* omaha: a set on the flop is raised, double-suited aces with kings are raised before it */
    setup_ai("KH KS 9D 4C", "KD 7C 2S", 4, 20, S_TIGHT, PG_OMAHA); ai_rates(10, 1, &r, &f); CHECK(r >= 50 && f == 0);
    setup_ai("AH KH AD KD", "", 4, 0, S_TIGHT, PG_OMAHA); at.total[0] = 5; at.total[1] = 10; ai_rates(10, 1, &r, &f); CHECK(r >= 70);
    setup_ai("9H 4D 2C 7S", "", 4, 0, S_TIGHT, PG_OMAHA); at.total[0] = 5; at.total[1] = 10; ai_rates(10, 1, &r, &f); CHECK(r <= 5 && f >= 30);
}

/* ---- whole hands ---------------------------------------------------------------------------------------- */
static uint8_t human_station(const Table *t, uint8_t can_raise, uint16_t to_call) { (void)t; (void)can_raise; (void)to_call; return A_CALL; }
static uint8_t human_random(const Table *t, uint8_t can_raise, uint16_t to_call) {
    uint16_t r = rng_below(10);
    (void)t; (void)to_call;
    return r < 2 ? A_FOLD : (r < 4 && can_raise) ? A_RAISE : A_CALL;
}
static uint16_t shove_want;
static uint8_t human_sizes(const Table *t, uint8_t can_raise, uint16_t to_call) {      /* random raise sizes, now and then all in */
    uint16_t r = rng_below(10);
    (void)to_call;
    shove_want = r < 2 ? (uint16_t)(t->bet[0] + t->chips[0]) : (uint16_t)(tbl_raise_to(t) + rng_below(6) * t->unit);
    return r < 1 ? A_FOLD : (r < 6) ? A_RAISE : (can_raise ? A_CALL : A_CALL);
}
static uint32_t nevents;
static void count_event(const Table *t, uint8_t ev, uint8_t seat, uint16_t amt) {
    nevents++; (void)seat; (void)amt;
    if (ev == EV_WIN) CHECK(t->showdown <= 1);
}

static uint16_t total_chips(const Table *t) {
    uint16_t s = 0; uint8_t i;
    for (i = 0; i < NSEAT; i++) s = (uint16_t)(s + t->chips[i] + t->total[i]);
    return s;
}

static void run_sim(uint8_t game, uint8_t kind, uint32_t hands) {
    static const Hooks hs = {human_station, count_event, 0};
    static const Hooks hr = {human_random, count_event, 0};
    static const Hooks hz = {human_sizes, count_event, &shove_want};
    const Hooks *hk = kind == 2 ? &hz : kind ? &hr : &hs;
    Table t;
    uint32_t i, sessions = 0, humanbust = 0, aibust = 0;
    uint32_t aiwin = 0;
    tbl_init(&t, game, hk);
    for (i = 0; i < hands; i++) {
        uint8_t go = tbl_hand(&t);
        CHECK(total_chips(&t) == NSEAT * START_CHIPS);
        if (!go) {
            sessions++;
            if (t.chips[0] == 0) humanbust++; else aibust++;
            aiwin += (uint32_t)(t.chips[1] + t.chips[2] + t.chips[3]);
            tbl_init(&t, game, hk);
        }
    }
    printf("game %u %s: %u hands, %u sessions, human busted %u, opponents busted %u, events %u\n",
           game, kind == 2 ? "sizes" : kind ? "random" : "caller", (unsigned)hands, (unsigned)sessions, (unsigned)humanbust, (unsigned)aibust, (unsigned)nevents);
    (void)aiwin;
}

/* the computer players against a calling station: they must make money */
static void test_skill(uint8_t game) {
    static const Hooks hs = {human_station, 0, 0};
    Table t;
    uint32_t i, rounds = 0;
    int32_t gain = 0;
    tbl_init(&t, game, &hs);
    for (i = 0; i < 2000; i++) {
        uint16_t before = t.chips[0];
        if (!tbl_hand(&t)) { tbl_init(&t, game, &hs); continue; }
        gain += (int32_t)t.chips[0] - (int32_t)before; rounds++;
    }
    printf("game %u: calling station result over %u hands: %d chips\n", game, (unsigned)rounds, (int)gain);
    CHECK(gain < 0);
}

int main(void) {
    uint8_t g;
    rng_seed(2024);
    test_ranking();
    test_seven_vs_brute();
#ifndef QUICK
    test_frequencies();
#endif
    test_omaha();
    test_pots();
    test_ai_aggression();
    for (g = 0; g < NPGAMES; g++) { run_sim(g, 0, SIM_HANDS); run_sim(g, 1, SIM_HANDS); run_sim(g, 2, SIM_HANDS); }
#ifndef QUICK
    for (g = 0; g < NPGAMES; g++) test_skill(g);
#endif
    printf("%d checks, %d failures\n", checks, fails);
    if (!fails) printf("ALL PASS\n");
    return fails != 0;
}
