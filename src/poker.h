#ifndef POKER_H
#define POKER_H
#include <stdint.h>
#include "cards.h"

#define NSEAT       4                   /* seat 0 is the human player, seats 1..3 are the computer opponents */
#define NPGAMES     2                   /* Texas Hold'em (the default, menu entry 0) and Omaha */
#define NHOLE       4                   /* most private cards a seat holds (Omaha) */
#define START_CHIPS 500
#define MAX_BETS    4                   /* fixed limit: a bet and up to three raises per round */

enum { PG_HOLDEM = 0, PG_OMAHA };
enum { A_FOLD = 0, A_CALL, A_RAISE, A_QUIT };            /* A_CALL is also "check" */
/* what a seat last did in the current betting round (display) */
enum { L_NONE = 0, L_FOLD, L_CHECK, L_CALL, L_BET, L_RAISE, L_ALLIN, L_BLIND };
enum { H_HIGH = 0, H_PAIR, H_TWOPAIR, H_TRIPS, H_STRAIGHT, H_FLUSH, H_FULL, H_QUADS, H_SFLUSH };
enum { S_TIGHT = 0, S_LOOSE, S_AGGRO, S_STATION };         /* computer playing styles (S_STATION: calls a lot, rarely raises) */
/* notifications for the user interface */
enum { EV_HAND = 1,                      /* a new hand starts (blinds/antes are in) */
       EV_REFRESH,                       /* cards or chips changed: repaint what differs */
       EV_THINK,                         /* seat is about to decide */
       EV_ACT,                           /* seat acted: t->last[seat], amount */
       EV_SHOW,                          /* showdown: the live hands are revealed */
       EV_WIN,                           /* seat won `amt` chips */
       EV_STREET };                      /* the next board cards are about to be dealt (not on the table yet): `amt` = the board size after them, 3 / 4 / 5 */

typedef uint32_t score_t;                /* category << 20 | five 4-bit ranks, larger = better */
#define SCORE_CAT(s) ((uint8_t)((s) >> 20))

typedef struct Table Table;
typedef struct {
    uint8_t (*human)(const Table *t, uint8_t can_raise, uint16_t to_call);   /* returns A_FOLD, A_CALL, A_RAISE or A_QUIT */
    void    (*event)(const Table *t, uint8_t ev, uint8_t seat, uint16_t amt);
    const uint16_t *want;                                                    /* optional: the bet level (total for the round) a human raise should reach; 0 = the usual size.
                                                                                A level above the usual may be any amount up to all-in. */
} Hooks;

struct Table {
    const Hooks *h;
    uint8_t  game;
    uint8_t  dealer;
    uint8_t  quit;                       /* the human left: the hand is played out silently, then the session ends */
    uint8_t  showdown;                   /* 1 once the live hands are revealed */
    uint8_t  out, folded, allin;         /* bit masks by seat; `out` seats have no chips left (and count as folded) */
    uint8_t  n[NSEAT];
    card_t   c[NSEAT][NHOLE];            /* the private cards */
    card_t   board[5];
    uint8_t  nb;
    uint8_t  style[NSEAT];
    uint16_t chips[NSEAT], total[NSEAT], bet[NSEAT], won[NSEAT];
    uint16_t cur, unit;                  /* bet to match in this round, size of one bet/raise */
    uint8_t  raises;
    uint8_t  last[NSEAT];
    score_t  score[NSEAT];               /* valid at the showdown */
    uint16_t hands;
    card_t   deck[52];
    uint8_t  dp;
};

/* ---- hand evaluation ---- */
score_t eval_n(const card_t *c, uint8_t n);                                  /* best five of n (0..7) cards */
score_t eval_omaha(const card_t *hole, uint8_t nh, const card_t *board, uint8_t nb);   /* exactly 2 hole + 3 board */
score_t eval_seat(const Table *t, uint8_t seat);                              /* the seat's best hand under the game's rules */

/* ---- table ---- */
void    tbl_init(Table *t, uint8_t game, const Hooks *h);
/* plays one hand; returns 1 if the session goes on (human has chips and at least one opponent too) */
uint8_t tbl_hand(Table *t);
uint8_t tbl_live(const Table *t);                                             /* seats still in the hand */
uint16_t tbl_pot(const Table *t);
uint16_t tbl_need(uint16_t to_call, uint16_t pot);   /* to_call * 100 / (pot + to_call) in 16 bits (no 32-bit division on the Z80) */
uint16_t tbl_raise_to(const Table *t);                                        /* bet level a raise would make in this round */

/* ---- computer player ---- */
uint8_t ai_equity(const Table *t, uint8_t seat);                              /* 0..100: chance to win the showdown (ties split) */
uint8_t ai_act(const Table *t, uint8_t seat, uint8_t can_raise, uint16_t to_call);

#endif
