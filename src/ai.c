#include "poker.h"

/* ---- hand strength -----------------------------------------------------------------------------------------
   Running a Monte Carlo rollout per decision costs seconds on a Z80, so the strength is a table lookup: the chance that
   the seat's hand beats ONE random opponent hand (by game, made hand and top rank, plus a bonus for draws). The chance
   against all live opponents is that value to the power of their number. */
static uint8_t rv(card_t c) { uint8_t r = RANK(c); return r ? (uint8_t)(r + 1) : 14; }

static uint8_t made_p(uint8_t omaha, uint8_t cat, uint8_t r) {      /* r = 2..14 */
    static const uint8_t flat[2][9] = {
        {0, 0, 78, 86, 91, 94, 97, 99, 100},                           /* hold'em */
        {0, 0, 62, 74, 84, 90, 97, 99, 100}};                          /* omaha: two pair is often not the best hand, but worth a bet */
    if (r < 2) r = 2;
    if (cat == H_HIGH) return (uint8_t)(omaha ? 5 + (r - 2) / 2 : 6 + (r - 2));
    if (cat == H_PAIR) return (uint8_t)(omaha ? 22 + (r - 2) : 36 + 2 * (r - 2));
    return flat[omaha][cat];
}

static uint8_t draw_bonus(const card_t *h, uint8_t nh, const card_t *b, uint8_t nb, uint8_t bonus_flush, uint8_t bonus_str) {
    uint8_t sc[4] = {0, 0, 0, 0}, i, v, best = 0, bonus = 0;
    uint16_t mask = 0;
    static const uint16_t W[10] = {62, 124, 248, 496, 992, 1984, 3968, 7936, 15872, 31744};
    for (i = 0; i < nh; i++) { sc[SUIT(h[i])]++; v = rv(h[i]); mask |= (uint16_t)(1u << v); }
    for (i = 0; i < nb; i++) { sc[SUIT(b[i])]++; v = rv(b[i]); mask |= (uint16_t)(1u << v); }
    for (i = 0; i < 4; i++) if (sc[i] == 4) best = 1;
    if (best) bonus = bonus_flush;
    mask |= (uint16_t)((mask >> 13) & 2u);
    for (i = 0; i < 10; i++) {
        uint16_t m = (uint16_t)(mask & W[i]);
        m = (uint16_t)((m & 0x5555u) + ((m >> 1) & 0x5555u)); m = (uint16_t)((m & 0x3333u) + ((m >> 2) & 0x3333u));
        m = (uint16_t)((m & 0x0F0Fu) + ((m >> 4) & 0x0F0Fu)); m = (uint16_t)((m & 0xFFu) + (m >> 8));
        if (m >= 4) { bonus = (uint8_t)(bonus + bonus_str); break; }
    }
    return bonus;
}

static uint8_t pre_holdem(const card_t *c) {              /* two hole cards, no board */
    uint8_t a = rv(c[0]), b = rv(c[1]), hi = a > b ? a : b, lo = a > b ? b : a, p;
    if (a == b) return (uint8_t)(46 + 3 * (a - 2));
    p = (uint8_t)(26 + 2 * (hi - 2) + (lo - 2));
    if (SUIT(c[0]) == SUIT(c[1])) p = (uint8_t)(p + 4);
    if (hi - lo <= 2) p = (uint8_t)(p + 2);
    return p;
}

static uint8_t pre_omaha(const card_t *c) {         /* four hole cards: the usual hand is worth 36 points, the scale is stretched to centre on 50 like hold'em's */
    uint8_t i, j, p = 28;
    for (i = 0; i < 4; i++) if (rv(c[i]) >= 10) p = (uint8_t)(p + 3);
    for (i = 0; i < 4; i++) for (j = (uint8_t)(i + 1); j < 4; j++) {
        if (rv(c[i]) == rv(c[j])) p = (uint8_t)(p + 4);
        else if (SUIT(c[i]) == SUIT(c[j])) p = (uint8_t)(p + 2);
    }
    return (uint8_t)(p < 30 ? 32 : p > 46 ? 80 : 50 + 3 * (p - 36));
}

static uint8_t single_p(const Table *t, uint8_t me) {
    const card_t *c = t->c[me];
    uint8_t p, cat, r;
    score_t sc;
    if (!t->nb) return t->game == PG_OMAHA ? pre_omaha(c) : pre_holdem(c);
    sc = eval_seat(t, me); cat = SCORE_CAT(sc); r = (uint8_t)((sc >> 16) & 15);
    if (t->game == PG_OMAHA) {
        p = made_p(1, cat, r);
        if (t->nb < 5) p = (uint8_t)(p + draw_bonus(c, 4, t->board, t->nb, t->nb == 3 ? 10 : 6, 4));
    } else {
        score_t bs = eval_n(t->board, t->nb);                      /* a hand that is only the board: the hole cards add nothing */
        uint8_t hi = rv(c[0]) > rv(c[1]) ? rv(c[0]) : rv(c[1]);
        if (cat <= H_TRIPS && SCORE_CAT(bs) == cat && ((bs >> 16) & 15) == r)
            p = cat == H_HIGH ? made_p(0, H_HIGH, hi) : cat == H_PAIR ? (uint8_t)(18 + hi / 2) : cat == H_TWOPAIR ? 45 : 55;
        else p = made_p(0, cat, r);
        if (t->nb < 5) p = (uint8_t)(p + draw_bonus(c, 2, t->board, t->nb, t->nb == 3 ? 16 : 9, 6));
    }
    return p > 99 ? 99 : p;
}

uint8_t ai_equity(const Table *t, uint8_t me) {
    uint8_t k, opp = (uint8_t)(tbl_live(t) - 1), p = single_p(t, me);
    uint16_t e = p;
    if (!opp) return 100;
    for (k = 1; k < opp; k++) e = (uint16_t)(e * p / 100);
    return (uint8_t)e;
}

/* tuning knobs (tests/ai_stats.c measures what they do) */
#ifndef AI_MULT
#define AI_MULT     6, 6, 4, 10
#endif
#ifndef AI_MARGIN
#define AI_MARGIN   6, 12, 8, 20
#endif
#ifndef AI_BLUFF
#define AI_BLUFF    6, 8, 20, 2
#endif
#ifndef AI_THR_MIN
#define AI_THR_MIN  {14, 14, 14, 14}, {32, 30, 24, 38}, {54, 52, 44, 64}   /* lowest bet threshold with 4, 3 and 2 players, per style (tight, loose, aggro, station): against fewer players a hand must beat a random one more often */
#endif
#ifndef AI_RERAISE
#define AI_RERAISE  80           /* % of the fair share added to the threshold for every raise already made */
#endif
#ifndef AI_CALL_POST
#define AI_CALL_POST 0
#endif
#ifndef AI_CALL_PRE
#define AI_CALL_PRE 12
#endif
#ifndef AI_INVESTED
#define AI_INVESTED 0
#endif

/* ---- betting decisions ------------------------------------------------------------------------------------
   The bet or raise threshold is a multiple of the fair share (100 / players) of the winning chance: the lower, the more
   aggressive. A re-raise needs more than a bet, and in a limit game a good made hand must be bet for value: a pair is
   worth a bet against few players, two pair or better almost always raises. Calls compare the chance with the pot odds. */
uint8_t ai_act(const Table *t, uint8_t seat, uint8_t can_raise, uint16_t to_call) {
    static const uint8_t mult[4]   = {AI_MULT};            /* bet when the equity is this many tenths of a fair share */
    static const uint8_t margin[4] = {AI_MARGIN};          /* how far below the pot odds a call is still made */
    static const uint8_t bluff[4]  = {AI_BLUFF};           /* % chance of a bet or raise without the hand */
    uint8_t live = tbl_live(t), st = t->style[seat], fair = (uint8_t)(100 / (live ? live : 1)), eq, thr, bonus;
    uint16_t extra = 0;
    uint16_t pot = tbl_pot(t), need;
    int16_t e;
    if (!to_call && !can_raise) return A_CALL;
    thr = (uint8_t)((uint16_t)fair * mult[st] / 10);
    { static const uint8_t tmin[3][4] = {AI_THR_MIN}; uint8_t m = tmin[live >= 4 ? 0 : live == 3 ? 1 : 2][st]; if (thr < m) thr = m; }
    if (thr > 85) thr = 85;
    if (to_call) extra = (uint16_t)((uint16_t)fair * AI_RERAISE / 100 * (t->raises - (t->nb == 0 ? 1 : 0)));    /* before the flop the blinds are the first bet */
    if (thr + extra > 80) extra = (uint16_t)(80 - thr);
    e = (int16_t)ai_equity(t, seat) + (int16_t)rng_below(9) - 4;
    eq = (uint8_t)(e < 0 ? 0 : e > 100 ? 100 : e);
    if (can_raise) {
        if (eq >= thr + extra) return A_RAISE;
        if (rng_below(100) < (to_call ? bluff[st] / 3 : bluff[st]) && (eq >= fair / 3 || (!to_call && t->nb))) return A_RAISE;   /* a free bet after the flop needs no hand: nobody has bet, so it may win the pot as it is */
    }
    if (!to_call) return A_CALL;
    need = tbl_need(to_call, pot);
    bonus = (uint8_t)(t->nb ? AI_CALL_POST : AI_CALL_PRE);      /* in the first round nobody gives up that easily, later not much easier */
    if (t->total[seat] >= to_call) bonus = (uint8_t)(bonus + AI_INVESTED);            /* already invested at least as much: it is not thrown away lightly */
    return (uint8_t)(eq + bonus + margin[st] >= need ? A_CALL : A_FOLD);
}
