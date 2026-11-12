#include "poker.h"

/* ---- hand evaluation ------------------------------------------------------------------------------ */

/* bit v of a rank mask = rank value v (2..14, ace high; the ace also counts as 1 for the wheel) */
static const uint16_t BITV[15] = {0, 0, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384};
/* the five bits of the straight with top card 5..14 (index top - 5) */
static const uint16_t STRM[10] = {62, 124, 248, 496, 992, 1984, 3968, 7936, 15872, 31744};

static uint8_t straight_top(uint16_t mask) {              /* top of the best straight in the mask, or 0 */
    uint8_t i;
    mask |= (uint16_t)((mask >> 13) & 2u);
    for (i = 10; i--; ) if ((mask & STRM[i]) == STRM[i]) return (uint8_t)(i + 5);
    return 0;
}

/* the score is built from three bytes: (category << 4 | r1), (r2 << 4 | r3), (r4 << 4 | r5); 16-bit work only until the end */
static score_t mk(uint8_t cat, uint8_t r1, uint8_t r2, uint8_t r3, uint8_t r4, uint8_t r5) {
    uint8_t b0 = (uint8_t)((cat << 4) | r1), b1 = (uint8_t)((r2 << 4) | r3), b2 = (uint8_t)((r4 << 4) | r5);
    return ((score_t)b0 << 16) | (uint16_t)(((uint16_t)b1 << 8) | b2);
}

score_t eval_n(const card_t *c, uint8_t n) {
    static uint8_t cnt[15], sc[4], d[7];                      /* static: absolute addressing is much faster than IX frames on the Z80 */
    uint8_t i, v, s, m = 0, q = 0, t1 = 0, p1 = 0, p2 = 0, fs = 4, k, top;
    uint16_t mask = 0, fm = 0;
    for (i = 0; i < 15; i++) cnt[i] = 0;
    for (i = 0; i < 4; i++) sc[i] = 0;
    for (i = 0; i < 7; i++) d[i] = 0;
    for (i = 0; i < n; i++) {
        v = RANK(c[i]); v = (uint8_t)(v ? v + 1 : 14);
        cnt[v]++; sc[SUIT(c[i])]++; mask |= BITV[v];
    }
    if (n >= 5) for (s = 0; s < 4; s++) if (sc[s] >= 5) fs = s;
    if (fs < 4) {                                              /* flush: maybe a straight flush */
        for (fm = 0, i = 0; i < n; i++) if (SUIT(c[i]) == fs) { v = RANK(c[i]); fm |= BITV[v ? v + 1 : 14]; }
        top = straight_top(fm);
        if (top) return mk(H_SFLUSH, top, 0, 0, 0, 0);
    }
    for (v = 14; v >= 2; v--) {
        k = cnt[v];
        if (!k) continue;
        d[m++] = v;
        if (k == 4 && !q) q = v;
        if (k >= 3 && !t1) t1 = v;
        else if (k >= 2) { if (!p1) p1 = v; else if (!p2) p2 = v; }
    }
    if (q) { for (i = 0; d[i] == q; i++) ; return mk(H_QUADS, q, d[i], 0, 0, 0); }
    if (t1 && p1) return mk(H_FULL, t1, p1, 0, 0, 0);
    if (fs < 4) {
        uint8_t f[5] = {0, 0, 0, 0, 0}, nf = 0;
        for (v = 14; v >= 2 && nf < 5; v--) if (fm & BITV[v]) f[nf++] = v;
        return mk(H_FLUSH, f[0], f[1], f[2], f[3], f[4]);
    }
    if (m >= 5) { top = straight_top(mask); if (top) return mk(H_STRAIGHT, top, 0, 0, 0, 0); }
    if (t1) {
        uint8_t k1 = 0, k2 = 0;
        for (i = 0; i < m; i++) if (d[i] != t1) { if (!k1) k1 = d[i]; else if (!k2) { k2 = d[i]; break; } }
        return mk(H_TRIPS, t1, k1, k2, 0, 0);
    }
    if (p1 && p2) { for (i = 0; d[i] == p1 || d[i] == p2; i++) ; return mk(H_TWOPAIR, p1, p2, d[i], 0, 0); }
    if (p1) {
        uint8_t kk[3] = {0, 0, 0}, nk = 0;
        for (i = 0; i < m && nk < 3; i++) if (d[i] != p1) kk[nk++] = d[i];
        return mk(H_PAIR, p1, kk[0], kk[1], kk[2], 0);
    }
    return mk(H_HIGH, d[0], d[1], d[2], d[3], d[4]);
}

score_t eval_omaha(const card_t *hole, uint8_t nh, const card_t *board, uint8_t nb) {
    card_t f[5];
    score_t best = 0, s;
    uint8_t a, b, i, j, k;
    if (nb < 3) return 0;
    for (a = 0; a + 1 < nh; a++)
        for (b = (uint8_t)(a + 1); b < nh; b++)
            for (i = 0; i + 2 < nb; i++)
                for (j = (uint8_t)(i + 1); j + 1 < nb; j++)
                    for (k = (uint8_t)(j + 1); k < nb; k++) {
                        f[0] = hole[a]; f[1] = hole[b]; f[2] = board[i]; f[3] = board[j]; f[4] = board[k];
                        s = eval_n(f, 5);
                        if (s > best) best = s;
                    }
    return best;
}

score_t eval_seat(const Table *t, uint8_t seat) {
    card_t all[7];
    uint8_t i, n = 0;
    if (t->game == PG_OMAHA) return eval_omaha(t->c[seat], t->n[seat], t->board, t->nb);
    for (i = 0; i < t->n[seat] && i < 2; i++) all[n++] = t->c[seat][i];
    for (i = 0; i < t->nb; i++) all[n++] = t->board[i];
    return eval_n(all, n);
}

/* ---- helpers --------------------------------------------------------------------------------------- */
#define BIT(s) ((uint8_t)(1u << (s)))

uint8_t tbl_live(const Table *t) {
    uint8_t s, n = 0;
    for (s = 0; s < NSEAT; s++) if (!(t->folded & BIT(s))) n++;
    return n;
}
static uint8_t actors(const Table *t) {
    uint8_t s, n = 0;
    for (s = 0; s < NSEAT; s++) if (!((t->folded | t->allin) & BIT(s))) n++;
    return n;
}
static uint8_t can_act(const Table *t, uint8_t s) { return !((t->folded | t->allin) & BIT(s)); }
static uint8_t next_seat(const Table *t, uint8_t s) {
    do s = (uint8_t)((s + 1) % NSEAT); while (t->out & BIT(s));
    return s;
}
uint16_t tbl_need(uint16_t to_call, uint16_t pot) {
    uint16_t d = (uint16_t)(pot + to_call);
    while (d > 655) { d >>= 1; to_call >>= 1; }            /* keep to_call * 100 inside 16 bits */
    return d ? (uint16_t)(to_call * 100u / d) : 0;
}

uint16_t tbl_pot(const Table *t) {
    uint8_t s; uint16_t p = 0;
    for (s = 0; s < NSEAT; s++) p = (uint16_t)(p + t->total[s]);
    return p;
}
uint16_t tbl_raise_to(const Table *t) { return t->cur < t->unit ? t->unit : (uint16_t)(t->cur + t->unit); }

static void emit(const Table *t, uint8_t ev, uint8_t seat, uint16_t amt) {
    if (t->h && t->h->event && !t->quit) t->h->event(t, ev, seat, amt);
}

static uint16_t pay(Table *t, uint8_t s, uint16_t amt) {
    if (amt > t->chips[s]) amt = t->chips[s];
    t->chips[s] = (uint16_t)(t->chips[s] - amt);
    t->total[s] = (uint16_t)(t->total[s] + amt);
    t->bet[s] = (uint16_t)(t->bet[s] + amt);
    if (!t->chips[s]) t->allin |= BIT(s);
    return amt;
}

static void deal(Table *t, uint8_t s) { t->c[s][t->n[s]++] = t->deck[t->dp++]; }
static void deal_board(Table *t) { t->board[t->nb++] = (card_t)(t->deck[t->dp++] | C_UP); }

static void round_begin(Table *t) {
    uint8_t s;
    t->cur = 0; t->raises = 0;
    for (s = 0; s < NSEAT; s++) { t->bet[s] = 0; t->last[s] = (t->folded & BIT(s)) ? L_FOLD : (t->allin & BIT(s)) ? L_ALLIN : L_NONE; }
}
static void set_cur(Table *t) {                 /* after forced bets: the highest one is the bet to match */
    uint8_t s;
    t->cur = 0;
    for (s = 0; s < NSEAT; s++) if (t->bet[s] > t->cur) t->cur = t->bet[s];
}

/* ---- one betting round ------------------------------------------------------------------------------ */
static void bet_round(Table *t, uint8_t first, uint16_t unit) {
    uint8_t need = 0, s = first, a, can_raise, i;
    uint16_t to_call, raise_to, amt;
    t->unit = unit;
    for (i = 0; i < NSEAT; i++) if (can_act(t, i)) need |= BIT(i);
    while (need && tbl_live(t) > 1) {
        for (i = 0; i < NSEAT && !(need & BIT(s)); i++) s = (uint8_t)((s + 1) % NSEAT);
        if (actors(t) < 2 && t->bet[s] >= t->cur) break;      /* alone against all-in players, nothing left to call */
        to_call = (uint16_t)(t->cur - t->bet[s]);
        raise_to = tbl_raise_to(t);
        can_raise = (uint8_t)(t->raises < MAX_BETS && t->chips[s] >= (uint16_t)(raise_to - t->bet[s]) && actors(t) > 1);
        emit(t, EV_THINK, s, 0);
        if (s == 0) a = (t->quit || !t->h || !t->h->human) ? A_FOLD : t->h->human(t, can_raise, to_call);
        else a = ai_act(t, s, can_raise, to_call);
        if (s == 0 && a == A_RAISE && t->h && t->h->want && *t->h->want && actors(t) > 1) {       /* the human chose a raise size */
            uint16_t w = *t->h->want, cap = (uint16_t)(t->bet[0] + t->chips[0]);
            if (w > cap) w = cap;
            if (w > t->cur && (w == cap || (can_raise && w >= raise_to))) { raise_to = w; can_raise = 1; }
        }
        if (a == A_QUIT) { t->quit = 1; a = A_FOLD; }
        if (a == A_FOLD && !to_call) a = A_CALL;
        if (a == A_RAISE && !can_raise) a = A_CALL;
        need = (uint8_t)(need & ~BIT(s));
        if (a == A_FOLD) { t->folded |= BIT(s); t->last[s] = L_FOLD; amt = 0; }
        else if (a == A_CALL) {
            amt = pay(t, s, to_call);
            t->last[s] = (uint8_t)(!to_call ? L_CHECK : (t->allin & BIT(s)) ? L_ALLIN : L_CALL);
        } else {
            amt = pay(t, s, (uint16_t)(raise_to - t->bet[s]));
            t->cur = raise_to; t->raises++;
            t->last[s] = (uint8_t)((t->allin & BIT(s)) ? L_ALLIN : t->raises == 1 ? L_BET : L_RAISE);
            for (i = 0; i < NSEAT; i++) if (i != s && can_act(t, i)) need |= BIT(i);
        }
        emit(t, EV_ACT, s, amt);
        s = (uint8_t)((s + 1) % NSEAT);
    }
}

/* ---- showdown: main pot and side pots ----------------------------------------------------------------- */
static void award(Table *t) {
    uint16_t rem[NSEAT], level, pot, share;
    uint8_t s, w, nw, k, elig[NSEAT];
    score_t best;
    for (s = 0; s < NSEAT; s++) { rem[s] = t->total[s]; t->won[s] = 0; }
    for (;;) {
        level = 0;
        for (s = 0; s < NSEAT; s++) if (rem[s] && (!level || rem[s] < level)) level = rem[s];
        if (!level) break;
        pot = 0; nw = 0; best = 0;
        for (s = 0; s < NSEAT; s++) {
            elig[s] = (uint8_t)(rem[s] && !(t->folded & BIT(s)));
            if (rem[s]) { uint16_t take = rem[s] < level ? rem[s] : level; pot = (uint16_t)(pot + take); rem[s] = (uint16_t)(rem[s] - take); }
        }
        for (s = 0; s < NSEAT; s++) if (elig[s] && (!nw || t->score[s] > best)) { best = t->score[s]; nw = 1; }
        for (nw = 0, s = 0; s < NSEAT; s++) if (elig[s] && t->score[s] == best) nw++;
        if (!nw) {                                              /* cannot happen with matched bets; keep the chips in play anyway */
            for (s = 0; s < NSEAT; s++) if (!(t->folded & BIT(s))) { elig[s] = 1; t->score[s] = best; nw++; break; }
            if (!nw) break;
        }
        share = (uint16_t)(pot / nw);
        k = (uint8_t)(pot - share * nw);                        /* odd chips go to the first winners left of the dealer */
        w = t->dealer;
        for (s = 0; s < NSEAT; s++) {
            w = (uint8_t)((w + 1) % NSEAT);
            if (elig[w] && t->score[w] == best) {
                uint16_t g = (uint16_t)(share + (k ? 1 : 0));
                if (k) k--;
                t->won[w] = (uint16_t)(t->won[w] + g);
                t->chips[w] = (uint16_t)(t->chips[w] + g);
            }
        }
    }
    for (s = 0; s < NSEAT; s++) t->total[s] = 0;
}

/* ---- the game flow: hold'em and omaha ----------------------------------------------------------------- */
static void deal_all(Table *t) {
    uint8_t s, i = 0;
    for (; i < NSEAT; i++) { s = (uint8_t)((t->dealer + 1 + i) % NSEAT); if (!(t->out & BIT(s))) deal(t, s); }
}

static void flow_blinds(Table *t, uint8_t hole) {          /* blinds 5/10, bets 10 before and after the flop, 20 on turn and river */
    uint8_t sb, bb, first, i, k, s, alive = 0;
    for (s = 0; s < NSEAT; s++) if (!(t->out & BIT(s))) alive++;
    if (alive == 2) { sb = t->dealer; bb = next_seat(t, sb); first = sb; }      /* heads up: the dealer is the small blind */
    else { sb = next_seat(t, t->dealer); bb = next_seat(t, sb); first = next_seat(t, bb); }
    round_begin(t);
    pay(t, sb, 5); t->last[sb] = L_BLIND;
    pay(t, bb, 10); t->last[bb] = L_BLIND;
    set_cur(t); t->raises = 1;
    for (i = 0; i < hole; i++) deal_all(t);
    emit(t, EV_HAND, 0, 0);
    bet_round(t, first, 10);
    for (i = 0; i < 3 && tbl_live(t) > 1; i++) {            /* flop, turn, river */
        emit(t, EV_STREET, 0, (uint16_t)(3 + i));
        for (k = 0; k < (i == 0 ? 3 : 1); k++) deal_board(t);
        emit(t, EV_REFRESH, 0, 0);
        round_begin(t);
        bet_round(t, next_seat(t, t->dealer), (uint16_t)(i ? 20 : 10));
    }
}

void tbl_init(Table *t, uint8_t game, const Hooks *h) {
    uint8_t s, base;
    t->h = h; t->game = game; t->out = 0; t->quit = 0; t->hands = 0;
    t->dealer = (uint8_t)rng_below(NSEAT);
    base = (uint8_t)rng_below(4);                              /* the three opponents get three different styles */
    for (s = 0; s < NSEAT; s++) {
        t->chips[s] = START_CHIPS; t->total[s] = t->bet[s] = t->won[s] = 0; t->n[s] = 0;
        t->style[s] = (uint8_t)((base + s) % 4);
    }
    t->nb = 0; t->folded = t->allin = 0; t->showdown = 0;
}

uint8_t tbl_hand(Table *t) {
    uint8_t s, alive = 0;
    for (s = 0; s < NSEAT; s++) if (!t->chips[s]) t->out |= BIT(s); else alive++;
    if (alive < 2 || (t->out & 1)) return 0;
    t->dealer = next_seat(t, t->dealer);
    t->folded = t->out; t->allin = 0; t->nb = 0; t->showdown = 0; t->hands++;
    for (s = 0; s < NSEAT; s++) { t->n[s] = 0; t->total[s] = t->bet[s] = t->won[s] = 0; t->score[s] = 0; t->last[s] = L_NONE; }
    deck_build(t->deck, 1, 4);
    deck_shuffle(t->deck, 52);
    t->dp = 0;
    flow_blinds(t, (uint8_t)(t->game == PG_OMAHA ? 4 : 2));
    /* cards for the board that were never dealt because everybody else was all in */
    if (tbl_live(t) > 1) while (t->nb < 5) deal_board(t);
    if (tbl_live(t) > 1) {
        t->showdown = 1;
        for (s = 0; s < NSEAT; s++) if (!(t->folded & BIT(s))) t->score[s] = eval_seat(t, s);
        emit(t, EV_SHOW, 0, 0);
    }
    award(t);
    for (s = 0; s < NSEAT; s++) if (t->won[s]) emit(t, EV_WIN, s, t->won[s]);
    for (s = 0; s < NSEAT; s++) if (!t->chips[s]) t->out |= BIT(s);
    alive = 0;
    for (s = 1; s < NSEAT; s++) if (!(t->out & BIT(s))) alive++;
    return (uint8_t)(!t->quit && !(t->out & 1) && alive > 0);
}
