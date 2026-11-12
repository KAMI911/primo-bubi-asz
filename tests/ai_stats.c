/* How the computer players behave: all four seats are played by the AI; every decision is logged by street, made hand and style.
   Prints how often each kind of hand bets/raises, calls and folds. Usage: ai_stats [game 0|1 as PG_*] [hands] */
#include <stdio.h>
#include <stdlib.h>
#include "../src/poker.h"
#define ai_act ai_act_real
#include "../src/ai.c"
#undef ai_act
#include "../src/poker.c"
#include "../src/cards.c"

/* [street 0..3][class][facing 0/1][action 0 fold, 1 call/check, 2 raise]; class after the flop: 0 high card, 1 pair, 2 two pair, 3 trips or better;
   before the flop the strength of the two (four) cards: 0 weak, 1 playable, 2 good, 3 premium */
static long st[4][5][2][3];
static long bystyle[4][2][3];
static long hands_played, raises_total;
static int only_live;                       /* 0: every decision, 2..4: only those with this many players in the hand */

uint8_t ai_act(const Table *t, uint8_t seat, uint8_t can_raise, uint16_t to_call) {
    uint8_t a = ai_act_real(t, seat, can_raise, to_call), street = t->nb == 0 ? 0 : (uint8_t)(t->nb - 2), cl = 0;
    if (a == A_RAISE && !can_raise) a = A_CALL;
    if (only_live && tbl_live(t) != only_live) return a;
    if (to_call == 0 && !can_raise) return a;
    if (t->nb >= 3) {
        score_t sc = eval_seat(t, seat); uint8_t cat = SCORE_CAT(sc);
        cl = cat >= H_TRIPS ? 3 : cat == H_TWOPAIR ? 2 : cat == H_PAIR ? 1 : 0;
    } else { uint8_t q = single_p(t, seat); cl = (uint8_t)(q >= 66 ? 3 : q >= 56 ? 2 : q >= 46 ? 1 : 0); }
    st[street][cl][to_call > 0][a == A_FOLD ? 0 : a == A_CALL ? 1 : 2]++;
    bystyle[t->style[seat]][to_call > 0][a == A_FOLD ? 0 : a == A_CALL ? 1 : 2]++;
    return a;
}
static uint8_t human_ai(const Table *t, uint8_t can_raise, uint16_t to_call) { return ai_act(t, 0, can_raise, to_call); }
static void ev(const Table *t, uint8_t e, uint8_t s, uint16_t a) { (void)t; (void)s; (void)a; (void)e; }

int main(int argc, char **argv) {
    static const Hooks hk = {human_ai, ev, 0};
    uint8_t game = argc > 1 ? (uint8_t)atoi(argv[1]) : PG_HOLDEM;
    long n = argc > 2 ? atol(argv[2]) : 20000, i;
    static const char *const sn[4] = {"tight", "loose", "aggro", "station"}, *const stn[4] = {"preflop", "flop", "turn", "river"};
    static const char *const cn[2][4] = {{"high card", "pair", "two pair", "trips+"}, {"weak", "playable", "good", "premium"}};
    Table t;
    uint8_t s, c, f;
    only_live = argc > 3 ? atoi(argv[3]) : 0;
    rng_seed(12345);
    tbl_init(&t, game, &hk);
    for (i = 0; i < n; i++) { if (!tbl_hand(&t)) tbl_init(&t, game, &hk); hands_played++; }
    printf("game %u, %ld hands, all seats played by the computer, decisions with %s\n", game, hands_played, only_live ? (only_live == 4 ? "4 players" : only_live == 3 ? "3 players" : "2 players") : "any number of players");
    printf("%-9s %-10s | no bet to face: check bet%% | facing a bet: fold%% call%% raise%%   (n)\n", "street", "hand");
    for (s = 0; s < 4; s++) for (c = 0; c < 4; c++) {
        long *u = st[s][c][0], *v = st[s][c][1], nu = u[0] + u[1] + u[2], nv = v[0] + v[1] + v[2];
        printf("%-9s %-10s | %6ld  %5.1f%% bet           | %5.1f %5.1f %5.1f  (%ld)\n", stn[s], cn[s == 0][c], nu, nu ? 100.0 * u[2] / nu : 0.0,
               nv ? 100.0 * v[0] / nv : 0.0, nv ? 100.0 * v[1] / nv : 0.0, nv ? 100.0 * v[2] / nv : 0.0, nv);
    }
    for (s = 0; s < 4; s++) {
        long *u = bystyle[s][0], *v = bystyle[s][1], nu = u[0] + u[1] + u[2], nv = v[0] + v[1] + v[2];
        printf("style %-8s: bets when free %.1f%%   facing a bet: fold %.1f%% call %.1f%% raise %.1f%%\n", sn[s], nu ? 100.0 * u[2] / nu : 0.0,
               nv ? 100.0 * v[0] / nv : 0.0, nv ? 100.0 * v[1] / nv : 0.0, nv ? 100.0 * v[2] / nv : 0.0);
    }
    (void)f; (void)raises_total;
    return 0;
}
