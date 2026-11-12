#include "play.h"
#include "gfx.h"
#include "layout.h"
#include "hall.h"
#include "hw.h"
#include "input.h"
#include "poker.h"
#include "ptext.h"
#include "ui.h"

/* The table screen. Nothing is repainted unless it differs from what is on the screen: every card has a "shown"
   code and every text field a cached string, and view_update() draws only the differences. */

static Table TB;
static uint8_t active, cur_game, over;

/* ---- layout ------------------------------------------------------------------------------------------ */
/* (the positions are in layout.h) */

/* the opponents get random names from this pool at the start of a session */
static const char *const name_pool[] = {
#ifdef LANG_HU
    T("ANDRAS", "ANDRÁS"), T("BELA", "BÉLA"), "CSABA", T("DEZSO", "DEZSŐ"), "ERZSI", "FERI", "HAJNI", T("ISTVAN", "ISTVÁN"),
    "KATA", "LACI", "OLGA", "ZSOLT"};
#else
    "ALICE", "BOB", "CARL", "DAISY", "EMMA", "FRANK", "HARRY", "IVY",
    "KATE", "LUKE", "MOLLY", "OLIVIA", "SAM", "WILL"};
#endif
#define NPOOL ((uint8_t)(sizeof name_pool / sizeof name_pool[0]))
static const char *names[NSEAT];

/* ---- cached text fields ------------------------------------------------------------------------------- */
enum { SL_TITLE = 0, SL_AI = 1 /* 3 per opponent */, SL_ME = 10, SL_POT, SL_HAND, SL_M0, SL_M1, SL_M2, SL_STAT, NSLOT };
/* cached text of each slot, packed: slot widths are 32 (title, me, hand, M0, status), 16 (pot), 10 (opponent fields), 11 and 12 (M1, M2), +1 for the end */
static const uint16_t toff[NSLOT + 1] = {0, 33, 44, 55, 66, 77, 88, 99, 110, 121, 132, 165, 182, 215, 248, 260, 273, 306};
static char    tcbuf[306];
#define tc(slot) (tcbuf + toff[slot])
static uint8_t tr[NSLOT];

static void put(uint8_t slot, uint8_t xb, uint8_t y, uint8_t w, const char *s, uint8_t rev) {
    char *c = tc(slot);
    uint8_t i, n = 0, same = (uint8_t)(tr[slot] == (uint8_t)(rev + 1));
    while (n < w && s[n]) n++;
    for (i = 0; i < w && same; i++) if (c[i] != (i < n ? s[i] : ' ')) same = 0;
    if (same) return;
    for (i = 0; i < w; i++) c[i] = i < n ? s[i] : ' ';
    c[w] = 0;
    tr[slot] = (uint8_t)(rev + 1);
    gfx_text(xb, y, c, rev);
}

/* a small string builder */
typedef struct { char s[34]; uint8_t n; } Sb;
static void sb_s(Sb *b, const char *s) { while (*s && b->n < 32) b->s[b->n++] = *s++; b->s[b->n] = 0; }
static void sb_n(Sb *b, uint16_t v) { b->n = (uint8_t)(b->n + gfx_u16(b->s + b->n, v)); b->s[b->n] = 0; }
static void sb_init(Sb *b) { b->n = 0; b->s[0] = 0; }

/* ---- cards ------------------------------------------------------------------------------------------- */
#define CODE_NONE 0xFF            /* nothing drawn here */
#define CODE_SLOT 0xFD            /* empty board position */
#define CODE_BACK 0x00
static uint8_t hold_show;      /* the showdown is not turned up on the screen yet (the commentators speak first) */
#define SHOWN(t) ((t)->showdown && !hold_show)
static card_t shown[NSEAT][NHOLE];
static card_t shown_b[5];

static const char *const hand_names[9] = {
    T("HIGH CARD", "MAGAS LAP"), T("PAIR", "PÁR"), T("TWO PAIR", "KÉT PÁR"), T("TRIPS", "DRILL"), T("STRAIGHT", "SOR"),
    T("FLUSH", "SZÍN"), T("FULL HOUSE", "FULL"), T("QUADS", "POKER"), T("STR. FLUSH", "SZÍNSOR")
};
const char *ui_hand_name(uint8_t cat) { return hand_names[cat % 9]; }

static void draw_code(uint8_t x4, uint8_t y, card_t code) {
    if (code == CODE_NONE) gfx_erase(x4, y, CARD_W4, CARD_H);
    else if (code == CODE_SLOT) { gfx_erase(x4, y, CARD_W4, CARD_H); gfx_slot(x4, y); }
    else gfx_card(x4, y, code, cfg.back);
}

static uint8_t card_x4(uint8_t s, uint8_t k) {
    return s == 0 ? (uint8_t)(ME_X4 + ME_STEP4 * k) : (uint8_t)(SEAT_W4 * (s - 1) + 3 * k);
}
static card_t want(const Table *t, uint8_t s, uint8_t k) {
    card_t c;
    if (k >= t->n[s] || (s && (t->folded & (1u << s)))) return CODE_NONE;
    c = t->c[s][k];
    if (s == 0 || SHOWN(t) || (c & C_UP)) return (card_t)((c & 0x3F) | C_UP);
    return CODE_BACK;
}

/* an opponent's cards (2 or 4, 3 units apart in an 18 unit wide strip; a card is 6 wide): face down until the showdown. The seat is drawn
   again whenever a card changes. */
static void upd_ai_cards(const Table *t, uint8_t s) {
    uint8_t k, x = (uint8_t)(SEAT_W4 * (s - 1)), same = 1;
    card_t w[NHOLE];
    for (k = 0; k < NHOLE; k++) {
        w[k] = want(t, s, k);
        if (w[k] != shown[s][k]) same = 0;
    }
    if (same) return;
    gfx_erase(x, Y_AICARD, SEAT_W4, CARD_H);
    for (k = 0; k < NHOLE; k++) {
        if (w[k] == CODE_NONE) continue;
        draw_code(x, Y_AICARD, w[k]);
        x = (uint8_t)(x + AI_STEP4);
    }
    for (k = 0; k < NHOLE; k++) shown[s][k] = w[k];
}

/* the player's own cards */
static void upd_seat_cards(const Table *t, uint8_t s) {
    uint8_t k, k0 = NHOLE;
    card_t w;
    if (s) { upd_ai_cards(t, s); return; }
    for (k = 0; k < NHOLE; k++) {
        w = want(t, 0, k);
        if (w != shown[0][k]) { if (k0 == NHOLE) k0 = k; if (w == CODE_NONE) k0 = 0; }
    }
    if (k0 == NHOLE) return;
    if (k0 == 0) {                                         /* cards left the seat: clear it and draw what remains */
        gfx_erase(0, Y_MECARD, 64, CARD_H);
        for (k = 0; k < NHOLE; k++) shown[0][k] = CODE_NONE;
    }
    for (k = k0; k < NHOLE; k++) {
        w = want(t, 0, k);
        if (w != CODE_NONE) draw_code(card_x4(0, k), Y_MECARD, w);
        shown[0][k] = w;
    }
}

static void upd_board(const Table *t) {
    uint8_t k;
    card_t w;
    for (k = 0; k < 5; k++) {
        w = k < t->nb ? (card_t)((t->board[k] & 0x3F) | C_UP) : CODE_SLOT;
        if (w != shown_b[k]) { draw_code((uint8_t)(BOARD_X4 + BOARD_STEP4 * k), Y_BOARD, w); shown_b[k] = w; }
    }
}

/* ---- text ------------------------------------------------------------------------------------------- */
static const char *last_word(uint8_t l) {
    switch (l) {
    case L_FOLD:  return T("FOLD", "ELDOB");
    case L_CHECK: return T("CHECK", "PASSZ");
    case L_CALL:  return T("CALL", "MEGAD");
    case L_BET:   return T("BET", "TESZ");
    case L_RAISE: return T("RAISE", "EMEL");
    case L_ALLIN: return T("ALL IN", "ALL IN");
    case L_BLIND: return T("BLIND", "VAK");
    default:      return "";
    }
}

/* "PAIR K" style description of a hand. It takes the category and the top rank, not the 32-bit score: sdcc passes a score_t
   argument of this function wrongly (the Z80 build showed TWO PAIR for a high card, QUADS for a pair). */
static void hand_text(Sb *b, uint8_t cat, uint8_t top) {
    static const char rk[16] = "0023456789TJQKA";
    char r[2];
    r[0] = rk[top]; r[1] = 0;
    sb_s(b, ui_hand_name(cat));
    if (cat == H_HIGH || cat == H_PAIR || cat == H_TRIPS || cat == H_QUADS || cat == H_STRAIGHT || cat == H_FLUSH) { sb_s(b, " "); sb_s(b, r); }
}

static void upd_ai_text(const Table *t, uint8_t s) {
    Sb b;
    uint8_t xb = (uint8_t)(11 * (s - 1)), sl = (uint8_t)(SL_AI + 3 * (s - 1));
    sb_init(&b); sb_s(&b, names[s]);
    while (b.n < 8) b.s[b.n++] = ' ';
    b.s[b.n] = 0;
    sb_s(&b, t->dealer == s ? "D" : "");
    put(sl, xb, Y_AI, 10, (t->out & (1u << s)) ? T("OUT", "KI") : b.s, 0);
    sb_init(&b); sb_s(&b, "$"); sb_n(&b, t->chips[s]);
    put((uint8_t)(sl + 1), xb, (uint8_t)(Y_AI + 8), 10, (t->out & (1u << s)) ? "" : b.s, 0);
    sb_init(&b);
    if (SHOWN(t) && !(t->folded & (1u << s))) {
        if (t->won[s]) { sb_s(&b, "+"); sb_n(&b, t->won[s]); sb_s(&b, " "); }
        sb_s(&b, ui_hand_name(SCORE_CAT(t->score[s])));
    } else if (t->last[s] && !(t->out & (1u << s))) {
        sb_s(&b, last_word(t->last[s]));
        if (t->last[s] != L_FOLD && t->last[s] != L_CHECK && t->bet[s]) { sb_s(&b, " "); sb_n(&b, t->bet[s]); }
    }
    put((uint8_t)(sl + 2), xb, (uint8_t)(Y_AI + 16), 10, b.s, (uint8_t)(t->won[s] != 0));
}

static void upd_me_text(const Table *t) {
    Sb b;
    sb_init(&b); sb_s(&b, cfg.name);
    while (b.n < 9) b.s[b.n++] = ' ';
    b.s[b.n] = 0;
    sb_s(&b, t->dealer == 0 ? "D  " : "   ");
    sb_s(&b, "$"); sb_n(&b, t->chips[0]);
    if (t->last[0]) { sb_s(&b, "  "); sb_s(&b, last_word(t->last[0])); if (t->bet[0] && t->last[0] != L_FOLD && t->last[0] != L_CHECK) { sb_s(&b, " "); sb_n(&b, t->bet[0]); } }
    put(SL_ME, 0, Y_ME, 32, b.s, 0);
}

static void upd_pot(const Table *t) {
    Sb b;
    uint8_t pad;
    sb_init(&b); sb_s(&b, T("POT ", "POT ")); sb_n(&b, tbl_pot(t));
    pad = (uint8_t)((16 - b.n) / 2);
    {
        char line[17]; uint8_t i;
        for (i = 0; i < 16; i++) line[i] = ' ';
        for (i = 0; i < b.n && i < 16; i++) line[pad + i] = b.s[i];
        line[16] = 0;
        put(SL_POT, 8, Y_POT, 16, tbl_pot(t) ? line : "", 0);
    }
}

static void upd_hand_line(const Table *t) {
    Sb b;
    sb_init(&b);
    if (t->n[0] && !(t->folded & 1u) && !(t->game == PG_OMAHA && t->nb < 3)) { score_t sc = eval_seat(t, 0); hand_text(&b, SCORE_CAT(sc), (uint8_t)((sc >> 16) & 15)); }
    put(SL_HAND, 0, Y_HAND, 32, b.s, 0);
}

static void set_title(const Table *t) {
    Sb b;
    sb_init(&b); sb_s(&b, ui_game_name(t->game)); sb_s(&b, "  #"); sb_n(&b, t->hands);
    {
        char line[33]; uint8_t i, pad = (uint8_t)((32 - b.n) / 2);
        for (i = 0; i < 32; i++) line[i] = ' ';
        for (i = 0; i < b.n; i++) line[pad + i] = b.s[i];
        line[32] = 0;
        put(SL_TITLE, 0, Y_TITLE, 32, line, 1);
    }
}

static void view_reset(void) {
    uint8_t s, k;
    for (s = 0; s < NSLOT; s++) { tc(s)[0] = 0; tr[s] = 0; }
    for (s = 0; s < NSEAT; s++) for (k = 0; k < NHOLE; k++) shown[s][k] = 0xFE;
    for (k = 0; k < 5; k++) shown_b[k] = 0xFE;
}

static void view_update(const Table *t) {
    uint8_t s;
    set_title(t);
    for (s = 1; s < NSEAT; s++) { upd_ai_text(t, s); upd_seat_cards(t, s); }
    upd_board(t);
    upd_pot(t);
    upd_seat_cards(t, 0);
    upd_me_text(t);
    upd_hand_line(t);
}

static void status(const char *s) {
    char line[33];
    uint8_t i;
    for (i = 0; i < 32 && s[i]; i++) line[i] = s[i];
    for (; i < 32; i++) line[i] = ' ';
    line[32] = 0;
    put(SL_STAT, 0, Y_STAT, 32, line, 1);
}

static void toggle_sound(void) { cfg.sound = (uint8_t)!cfg.sound; status(cfg.sound ? T("SOUND ON", "HANG BE") : T("SOUND OFF", "HANG KI")); }
static uint8_t ask_leave(void) {                          /* BRK or M: back to the menu only after a yes (C, SPACE, or BRK/M again; F or any other key: no) */
    uint8_t ev;
    status(T("LEAVE THE TABLE? C: YES  F: NO", "KILÉPSZ? C: IGEN  F: NEM"));
    input_wait_release();
    for (;;) {
        hw_wait_frame();
        ev = input_poll();
        if (ev == KEY_CALL || ev == KEY_SELECT || ev == KEY_BACK || ev == KEY_MENU) return 1;
        if (ev == KEY_FOLD) return 0;
    }
}

/* ---- timing and input ------------------------------------------------------------------------------------ */
static void pause_frames(uint8_t n) {                    /* a key press cuts the wait short */
    while (n--) { hw_wait_frame(); if (input_poll() != KEY_NONE) { return; } }
}

static void do_pause(void) {
    uint8_t ev;
    gfx_clear();
    gfx_text_c(80, T("PAUSED", "SZÜNET"), 1);
    gfx_text_c(100, T("ANY KEY CONTINUES", "BÁRMELY GOMB FOLYTATJA"), 0);
    input_wait_release();
    do { hw_wait_frame(); ev = input_poll(); } while (ev == KEY_NONE);
    input_wait_release();
    gfx_clear();
    view_reset();
}

static void show_chance(const Table *t, uint16_t to_call) {
    Sb b;
    uint8_t eq;
    uint16_t need;
    status(T("THINKING...", "SZÁMOLOK..."));
    eq = ai_equity(t, 0);
    sb_init(&b); sb_s(&b, T("WIN CHANCE ", "ESÉLY ")); sb_n(&b, eq); sb_s(&b, "%");
    if (to_call) { need = tbl_need(to_call, tbl_pot(t)); sb_s(&b, T("  NEED ", "  KELL ")); sb_n(&b, need); sb_s(&b, "%"); }
    status(b.s);
}

/* ---- the human's decisions -------------------------------------------------------------------------------- */
static void menu_clear(void) {
    put(SL_M0, MENU_X0, Y_MENU, 8, "", 0);
    put(SL_M1, MENU_X1, Y_MENU, 12, "", 0);
    put(SL_M2, MENU_X2, Y_MENU, 12, "", 0);
}

static uint8_t confirm_allin(const Table *t);
static uint16_t raise_want;                  /* the bet level the player picked for a raise (Hooks.want) */

static uint8_t h_human(const Table *t, uint8_t can_raise, uint16_t to_call) {
    uint8_t items[3], n = 0, sel = 0, i, ev, res, nl = 0, lvl = 0, shove;
    uint16_t lv[8], cap = (uint16_t)(t->bet[0] + t->chips[0]);
    Sb b;
    static const uint8_t mult[6] = {0, 1, 2, 3, 5, 7};          /* raise sizes in units above the usual one */
    view_update(t);
    for (shove = 0, i = 1; i < NSEAT; i++) if (!((t->folded | t->allin | t->out) & (1u << i))) shove = 1;   /* somebody can still call */
    if (t->chips[0] <= to_call) shove = 0;
    if (can_raise) for (i = 0; i < 6; i++) { uint16_t v = (uint16_t)(tbl_raise_to(t) + mult[i] * t->unit); if (v < cap) lv[nl++] = v; }
    if (shove) lv[nl++] = cap;                                   /* the top level is all in */
    if (to_call) items[n++] = A_FOLD;
    sel = n; items[n++] = A_CALL;
    if (nl) items[n++] = A_RAISE;
    status(to_call ? T("YOUR TURN F FOLD C CALL R RAISE", "TE JÖSSZ: F ELDOB C MEGAD R EMEL") : T("YOUR TURN C CHECK R BET", "TE JÖSSZ: C PASSZ  R TESZ"));
    input_wait_release();
    for (;;) {
        for (i = 0; i < n; i++) {
            sb_init(&b);
            if (items[i] == A_FOLD) { sb_s(&b, T("F:FOLD", "F:ELDOB")); put(SL_M0, MENU_X0, Y_MENU, 8, b.s, (uint8_t)(sel == i)); }
            else if (items[i] == A_CALL) {
                if (to_call) { sb_s(&b, T("C:CALL ", "C:MEGAD ")); sb_n(&b, to_call); } else sb_s(&b, T("C:CHECK", "C:PASSZ"));
                put(SL_M1, MENU_X1, Y_MENU, 12, b.s, (uint8_t)(sel == i));
            } else {
                if (lv[lvl] == cap) sb_s(&b, T("R:ALLIN ", "R:ALLIN "));
                else sb_s(&b, to_call ? T("R:RAISE ", "R:EMEL ") : T("R:BET ", "R:TESZ "));
                sb_n(&b, (uint16_t)(lv[lvl] - t->bet[0]));
                put(SL_M2, MENU_X2, Y_MENU, 12, b.s, (uint8_t)(sel == i));
            }
        }
        hw_wait_frame();
        ev = input_poll();
        if (ev == KEY_NONE) continue;
        res = 0xFF;
        switch (ev) {
        case KEY_LEFT: sel = sel ? (uint8_t)(sel - 1) : (uint8_t)(n - 1); break;      /* wraps around: left of the first is the last */
        case KEY_RIGHT: sel = (uint8_t)(sel + 1 < n ? sel + 1 : 0); break;
        case KEY_UP: if (nl && lvl + 1 < nl) lvl++; break;
        case KEY_DOWN: if (lvl) lvl--; break;
        case KEY_SELECT: res = items[sel]; break;
        case KEY_FOLD: if (to_call) res = A_FOLD; break;
        case KEY_CALL: res = A_CALL; break;
        case KEY_RAISE: if (nl) res = A_RAISE; else { ui_snd(1); status(T("NO RAISE POSSIBLE", "NEM EMELHETSZ")); } break;
        case KEY_HINT: show_chance(t, to_call); break;
        case KEY_SOUND: toggle_sound(); break;
        case KEY_PAUSE: do_pause(); view_update(t); break;
        case KEY_BACK: case KEY_MENU:
            if (ask_leave()) res = A_QUIT;           /* the hand is folded and played out, then the menu: CONTINUE picks the session up again */
            else status(to_call ? T("YOUR TURN F FOLD C CALL R RAISE", "TE JÖSSZ: F ELDOB C MEGAD R EMEL") : T("YOUR TURN C CHECK R BET", "TE JÖSSZ: C PASSZ  R TESZ"));
            break;
        default: break;
        }
        if (res == A_RAISE) {
            if (lv[lvl] == cap && !confirm_allin(t)) { res = 0xFF; input_wait_release(); status(T("UP/DOWN: RAISE SIZE", "FEL/LE: EMELÉS MÉRTÉKE")); }
            else raise_want = lv[lvl];
        }
        if (res != 0xFF) { menu_clear(); ui_snd(0); return res; }
    }
}

/* ---- Gyorgy and Klari: now and then they comment on the game in a comic speech bubble at the bottom right ----------------
   The picture covers part of the table; afterwards that part is cleared and only what was under it is repainted (no RAM for
   a saved copy: the colour model has none to spare). */
/* the comments are in src/chat.txt, packed by tools/gen_chat.py into build/chat_{en,hu}.h: the categories Q_*, the table ch_quip (category | 16 if
   Klari speaks, text, her answer: 0..253 her own line, CH_RR one of the replies at random, 255 the speaker is alone), the packed strings */
#ifdef LANG_HU
#include "chat_hu.h"
#else
#include "chat_en.h"
#endif
static char chbuf[CH_MAXLEN + 1 + 16];                     /* room for the player's name (up to 8 letters) twice */

/* string number n, unpacked into chbuf (valid until the next call): strings are 0-ended, codes 128.. stand for a pair of codes */
static char *ch_put(char *o, uint8_t c) {
    if (c == '%') { const char *nm = cfg.name; while (*nm) *o++ = *nm++; return o; }   /* % is the player's name */
    if (c < 128) { *o++ = (char)c; return o; }
    o = ch_put(o, ch_pair[c - 128][0]);
    return ch_put(o, ch_pair[c - 128][1]);
}
static const char *chat_text(uint8_t n) {
    const uint8_t *p = ch_blob;
    char *o = chbuf;
    while (n--) while (*p++) ;
    while (*p) o = ch_put(o, *p++);
    *o = 0;
    return chbuf;
}
const char *play_rules_line(uint8_t group, uint8_t k) { return k < ch_rules[group][1] ? chat_text((uint8_t)(ch_rules[group][0] + k)) : 0; }
#define NQUIPS CH_NQUIPS
static uint8_t said;           /* comments so far in this hand */
#define DRUNK_MAX 5            /* the talkative pair: at most this many per hand, the last one is kept for the end of the hand (who won) */
#define CHAT_MAX  3            /* the normal setting: at most this many per hand, likewise the last one for the end of the hand */
#define CHAT_HOT_POT 150       /* the overheated comments on a raise ('what excitement!') only come when this much is in the pot, or from the turn on */
#define DRUNK_X   2            /* the talkative pair comments this many times as often */

static uint8_t quip_wrap(const char *s, uint8_t *st, uint8_t *ln) {   /* greedy word wrap to at most 4 lines of 22 characters: start and length of each line */
    uint8_t n = 0, len = 0, w, p = 0, ws;
    st[0] = 0; ln[0] = 0;
    for (;;) {
        while (s[p] == ' ') p++;
        if (!s[p]) break;
        ws = p;
        while (s[p] && s[p] != ' ') p++;
        w = (uint8_t)(p - ws);
        if (w > 22) w = 22;
        if (len && len + 1 + w > 22) { if (++n == 4) return 4; len = 0; }
        if (!len) st[n] = ws;
        len = (uint8_t)(ws + w - st[n]);
        ln[n] = len;
    }
    return (uint8_t)(n + 1);
}

static uint8_t skip_key(void) { return input_poll() != KEY_NONE; }

/* one bubble: it pops up in three sizes, the text is typed letter by letter (a key shows it all), then it stays a while;
   returns nothing, a key during the hold ends it */
static void say(uint8_t fx4, const char *text) {
    char one[2];
    uint8_t st[4], ll[4], n = quip_wrap(text, st, ll), i, j, fast = 0, tick = 0;
    uint16_t f, len = 0;
    while (text[len]) len++;
    if (!hw_is_color()) gfx_xor_frame(fx4, QF_Y, 12, 48);        /* the speaker gets a frame (monochrome: in colour the frame would turn the colours around) */
    gfx_fill(QB_X4, QB_Y, QB_W4, QB_H + 6, 0);
    gfx_bubble(QB_X4, QB_Y, QB_W4, QB_H, fx4 > QF_X4 ? 226 : 178);   /* the tail points at the speaker's portrait (alone: always the right one) */
    ui_snd(2);
    input_wait_release();
    one[1] = 0;
    for (i = 0; i < n; i++)
        for (j = 0; j < ll[i]; j++) {
            one[0] = text[st[i] + j];
            gfx_text((uint8_t)(8 + j), (uint8_t)(QB_Y + 22 - 5 * n + 10 * i), one, 1);
            if (!fast) {
                if ((++tick & 1) && cfg.sound) hw_beep(24, 1);
                hw_wait_frame(); hw_wait_frame();
                if (skip_key()) fast = 1;
            }
        }
    input_wait_release();
    for (f = 0; f < 100 + 4 * len; f++) { hw_wait_frame(); if (f > 10 && skip_key()) break; }   /* any key skips */
    input_wait_release();
    gfx_fill(QB_X4, QB_Y, QB_W4, QB_H + 6, 0);
    if (!hw_is_color()) gfx_xor_frame(fx4, QF_Y, 12, 48);
}

/* a comment: usually one of the two alone at the right edge, a dialogue (both, one reaction) when the quip has a reply */
static void overlay_end(const Table *t) {                       /* clear the pictures' area, repaint what was under them */
    uint8_t i;
    gfx_fill(QB_X4, QB_Y, QB_W4, QB_H + 6, 0);
    gfx_fill(QF_X4, QF_Y, 24, 48, 0);
    for (i = 0; i < 5; i++) shown_b[i] = 0xFE;
    for (i = 0; i < NHOLE; i++) if (card_x4(0, i) + CARD_W4 > QF_X4) shown[0][i] = 0xFE;       /* only the player's cards the portraits covered */
    for (i = SL_ME; i <= SL_M2; i++) { tc(i)[0] = 0; tr[i] = 0; }
    view_update(t);
}

/* all in: Gyorgy (mostly) or Klari asks back; C, SPACE or R again says yes, F or M says no. Without the commentators a line in the status bar asks. */
static uint8_t confirm_allin(const Table *t) {
    char one[2];
    uint8_t st[4], ll[4], who = (uint8_t)(rng_below(100) >= 70), n, i, j, ev, yes = 0;
    const char *txt = chat_text((uint8_t)(CH_ASK0 + (who ? 3 + rng_below(2) : rng_below(3))));
    if (cfg.chat) {
        n = quip_wrap(txt, st, ll);
        if (n > 3) n = 3;
        one[1] = 0;
        gfx_face((uint8_t)(QF_X4 + 12), QF_Y, who);
        gfx_bubble(QB_X4, QB_Y, QB_W4, QB_H, 226);                  /* the one who asks is alone, in the right portrait slot */
        for (i = 0; i < n; i++) for (j = 0; j < ll[i]; j++) { one[0] = txt[st[i] + j]; gfx_text((uint8_t)(8 + j), (uint8_t)(QB_Y + 4 + 10 * i), one, 1); }
        gfx_text(8, (uint8_t)(QB_Y + 30), T("C: YES   F: NO", "C: IGEN   F: NEM"), 1);
        ui_snd(1);
    } else status(T("ALL IN? C: YES  F: NO", "ALL IN? C: IGEN  F: NEM"));
    input_wait_release();
    for (;;) {
        hw_wait_frame();
        ev = input_poll();
        if (ev == KEY_CALL || ev == KEY_SELECT || ev == KEY_RAISE) { yes = 1; break; }
        if (ev == KEY_FOLD || ev == KEY_BACK || ev == KEY_MENU) break;
    }
    if (cfg.chat) overlay_end(t);
    return yes;
}

#define RECENT_Q 6             /* the last comments and Klari's last replies are not used again at once */
#define RECENT_R 6
static uint8_t recent_q[RECENT_Q] = {255, 255, 255, 255, 255, 255}, recent_r[RECENT_R] = {255, 255, 255, 255, 255, 255};
static uint8_t was_recent(const uint8_t *ring, uint8_t n, uint8_t v) { uint8_t i; for (i = 0; i < n; i++) if (ring[i] == v) return 1; return 0; }
static void remember(uint8_t *ring, uint8_t n, uint8_t v) { uint8_t i; for (i = (uint8_t)(n - 1); i; i--) ring[i] = ring[i - 1]; ring[0] = v; }

static uint8_t pick_reply(uint8_t personal) {                         /* a random reply that is not one of the last few; the ones with the player's name only if personal */
    uint8_t r, tries = 0;
    do r = (uint8_t)(CH_REPLY0 + rng_below(personal ? CH_NREPLY : CH_NPLAIN)); while (was_recent(recent_r, RECENT_R, r) && ++tries < 20);
    remember(recent_r, RECENT_R, r);
    return r;
}

static void quip_show(const Table *t, const uint8_t *q) {
    uint8_t re = q[2], who = (uint8_t)((q[0] >> 4) & 1);
    if (re != 255) gfx_face(QF_X4, QF_Y, 0);
    gfx_face((uint8_t)(QF_X4 + 12), QF_Y, re != 255 ? 1 : who);
    say(re != 255 ? (uint8_t)(QF_X4 + 12 * who) : (uint8_t)(QF_X4 + 12), chat_text(q[1]));
    if (re != 255) say((uint8_t)(QF_X4 + 12 * (1 - who)), chat_text(re == CH_RR ? pick_reply((uint8_t)!(t->folded & 1u)) : re));
    overlay_end(t);
}

void play_quip_test(uint8_t n) { quip_show(&TB, ch_quip[n % NQUIPS]); }

/* one comment of a category: forced, or with `pct` percent chance while fewer than CHAT_MAX (talkative: DRUNK_MAX) were said in this hand */
static void chat(const Table *t, uint8_t cat, uint8_t pct, uint8_t force) {
    uint8_t i, n = 0, k, pass, skip = (uint8_t)(t->folded & 1u && cat != Q_FOLD);       /* after the player folded nobody speaks to them by name any more (the reaction to the fold itself is fine) */
    /* commentators 1: now and then, up to CHAT_MAX per hand; 2 (talkative): DRUNK_X times as often, up to DRUNK_MAX per hand, but the
       comments during the play stop one short, so the end of the hand (the winner: AIWIN, MEWIN, BROKE) can still be commented */
    if (!cfg.chat) return;
    { uint8_t mx = cfg.chat == 2 ? DRUNK_MAX : CHAT_MAX; if (said >= (force || cat == Q_AIWIN || cat == Q_MEWIN || cat == Q_BROKE ? mx : mx - 1)) return; }
    if (!force && rng_below(100) >= (cfg.chat == 2 ? (unsigned)DRUNK_X * pct : pct)) return;
    for (pass = 0; pass < 2; pass++) {                              /* first only the lines not said lately; if there are none, any */
        n = 0;
        for (i = 0; i < NQUIPS; i++) if ((ch_quip[i][0] & 15) == cat && (pass || !was_recent(recent_q, RECENT_Q, i)) && !(skip && (ch_quip[i][0] & 32))) n++;
        if (!n) continue;
        k = (uint8_t)rng_below(n);
        for (i = 0; i < NQUIPS; i++)
            if ((ch_quip[i][0] & 15) == cat && (pass || !was_recent(recent_q, RECENT_Q, i)) && !(skip && (ch_quip[i][0] & 32)) && !k--) { said++; remember(recent_q, RECENT_Q, i); quip_show(t, ch_quip[i]); return; }
    }
}

static void pick_names(void) {                                     /* three different random names */
    uint8_t s, k, i;
    for (s = 1; s < NSEAT; s++) {
        do { k = (uint8_t)rng_below(NPOOL); for (i = 1; i < s && names[i] != name_pool[k]; i++) ; } while (i < s);
        names[s] = name_pool[k];
    }
}

static uint8_t has_bubi_asz(const Table *t, uint8_t seat) {       /* a jack and an ace of the same suit among the seat's cards and the board */
    uint8_t i, m[4] = {0, 0, 0, 0}, c;
    for (i = 0; i < t->n[seat] + t->nb; i++) {
        c = i < t->n[seat] ? t->c[seat][i] : t->board[i - t->n[seat]];
        if (RANK(c) == 10) m[SUIT(c)] |= 1; else if (RANK(c) == 0) m[SUIT(c)] |= 2;
    }
    return m[0] == 3 || m[1] == 3 || m[2] == 3 || m[3] == 3;
}

#ifdef HOST
uint8_t play_bubi_test(const Table *t, uint8_t seat) { return has_bubi_asz(t, seat); }
#endif
static uint8_t bubi_done;      /* the Bubi Asz line was said in this hand */
static uint8_t river_done, turn_done;   /* the turn and river lines were said in this hand */
static uint16_t cur_prev;      /* the bet to match when the last seat started to think: an all in above it is a push, otherwise a call */
static void bubi_check(const Table *t) {                          /* the player's own Bubi Asz: said once, when it is there on the screen (a new card is on the table); never at the showdown */
    if (!bubi_done && !(t->folded & 1u) && has_bubi_asz(t, 0)) { bubi_done = 1; chat(t, Q_BUBIASZ, 0, 1); }
}

/* ---- notifications from the engine ----------------------------------------------------------------------------- */
static const char *seat_name(uint8_t s) { return s ? names[s] : cfg.name; }

static void ev_draw(const Table *t, uint8_t ev, uint8_t seat, uint16_t amt) {
    Sb b;
    switch (ev) {
    case EV_HAND:
        view_update(t);
        status(T("NEW HAND", "ÚJ OSZTÁS"));
        ui_snd(0);
        pause_frames(20);
        break;
    case EV_REFRESH:
        view_update(t);
        pause_frames(18);
        break;
    case EV_THINK:
        if (seat) {
            put((uint8_t)(SL_AI + 3 * (seat - 1) + 2), (uint8_t)(11 * (seat - 1)), (uint8_t)(Y_AI + 16), 10, "...", 0);   /* only the opponent's own field: the status bar changes once per move */
        }
        break;
    case EV_ACT:
        view_update(t);
        sb_init(&b); sb_s(&b, seat_name(seat)); sb_s(&b, " "); sb_s(&b, last_word(t->last[seat]));
        if (amt) { sb_s(&b, " "); sb_n(&b, amt); }
        status(b.s);
        ui_snd(t->last[seat] == L_FOLD ? 1 : 0);
        pause_frames(seat ? 32 : 10);
        break;
    case EV_SHOW:
        view_update(t);
        status(T("SHOWDOWN", "FELFORDÍTÁS"));
        pause_frames(50);
        break;
    case EV_WIN:
        view_update(t);
        sb_init(&b); sb_s(&b, seat_name(seat)); sb_s(&b, T(" WINS ", " NYER ")); sb_n(&b, amt);
        if (t->showdown) { sb_s(&b, " - "); hand_text(&b, SCORE_CAT(t->score[seat]), (uint8_t)((t->score[seat] >> 16) & 15)); }
        status(b.s);
        ui_snd(seat == 0 ? 3 : 2);
        pause_frames(70);
        break;
    default: break;
    }
}

/* the commentators react after the event is drawn; a separate function so the drawing's buffers are not on the stack while a bubble is up */
static void ev_chat(const Table *t, uint8_t ev, uint8_t seat, uint16_t amt) {
    uint8_t i;
    switch (ev) {
    case EV_HAND: said = 0; bubi_done = 0; river_done = turn_done = 0; bubi_check(t); break;
    case EV_REFRESH:
        bubi_check(t);
        break;
    case EV_STREET:                                              /* said before the card is dealt, not after it is already on the table */
        if (amt == 5 && !river_done) { river_done = 1; chat(t, Q_RIVER, 12, 0); }
        else if (amt == 4 && !turn_done) { turn_done = 1; chat(t, Q_TURN, 10, 0); }
        break;
    case EV_THINK: cur_prev = t->cur; break;
    case EV_ACT:
        if (t->last[seat] == L_ALLIN) {
            if (t->cur > cur_prev) chat(t, Q_ALLIN, 0, 1);
            else if (t->nb >= 4 || tbl_pot(t) >= 2 * CHAT_HOT_POT) chat(t, Q_ALLINC, 0, 1);   /* 'called!' only late in the hand or with a big pot */
        }
        else if (seat == 0 && t->last[0] == L_FOLD) chat(t, Q_FOLD, 55, 0);
        else if (!(t->nb >= 4 || tbl_pot(t) >= CHAT_HOT_POT)) break;           /* early in the hand and a small pot: nothing to get excited about */
        else if (t->last[seat] == L_RAISE) chat(t, Q_IDLE, t->nb == 5 ? 45 : 30, 0);
        else if (cfg.chat == 2 && seat && t->last[seat] != L_BLIND) chat(t, Q_IDLE, t->nb == 5 ? 30 : 20, 0);        /* the talkative pair also comments on the others' calls and bets */
        break;
    case EV_SHOW:
        break;
    case EV_WIN:
        if (seat && t->showdown && SCORE_CAT(t->score[seat]) == H_HIGH) { chat(t, Q_BLUFF, 75, 0); break; }   /* only when somebody really WINS with nothing at all */
        if (seat == 0) { if (amt > 200) chat(t, Q_MEWIN, 65, amt > 500); }   /* praise: sometimes above 200, always above 500 */
        else if (t->chips[0] < 250) chat(t, Q_BROKE, 65, 0);
        else chat(t, Q_AIWIN, 45, 0);
        break;
    default: break;
    }
}

static void h_event(const Table *t, uint8_t ev, uint8_t seat, uint16_t amt) {
    if (ev == EV_SHOW && cfg.chat) {                              /* before the hands are turned up: the build-up, and the player's own good hand */
        hold_show = 1;
        chat(t, Q_SHOW, 40, 0);
        if (!(t->folded & 1u) && SCORE_CAT(t->score[0]) >= H_STRAIGHT) chat(t, Q_NICE, 75, 0);
        hold_show = 0;
    }
    ev_draw(t, ev, seat, amt);
    if (cfg.chat) ev_chat(t, ev, seat, amt);
}

static const Hooks hooks = {h_human, h_event, &raise_want};

/* ---- session --------------------------------------------------------------------------------------------------- */
uint8_t play_active(void) { return active; }
uint8_t play_game_index(void) { return cur_game; }

static void record_result(void) {
    uint8_t rank = hall_rank(cur_game, TB.chips[0]);
    if (rank != 0xFF) hall_add(cur_game, rank, cfg.name, TB.chips[0], TB.hands);
}

static void end_screen(void) {
    Sb b;
    uint8_t rank = hall_rank(cur_game, TB.chips[0]), k;
    gfx_clear();
    view_reset();
    if (TB.chips[0]) { gfx_text2(2, 20, T("YOU WIN!", "NYERTÉL!")); gfx_text_c(52, T("THE TABLE IS CLEANED OUT", "KINYERTED MINDEN ZSETONT"), 0); ui_snd(3); }
    else { gfx_text2(2, 20, T("GAME OVER", "VÉGE")); gfx_text_c(52, T("YOU ARE OUT OF CHIPS", "ELFOGYOTT A ZSETONOD"), 0); ui_snd(1); }
    sb_init(&b); sb_s(&b, T("CHIPS ", "ZSETON ")); sb_n(&b, TB.chips[0]); sb_s(&b, T("  HANDS ", "  OSZTÁS ")); sb_n(&b, TB.hands);
    gfx_text_c(76, b.s, 0);
    if (rank != 0xFF) {
        sb_init(&b); sb_s(&b, T("NEW RECORD! PLACE ", "ÚJ REKORD! HELY ")); k = (uint8_t)('1' + rank); b.s[b.n++] = (char)k; b.s[b.n] = 0;
        gfx_text_c(96, b.s, 1);
        hall_add(cur_game, rank, cfg.name, TB.chips[0], TB.hands);
    }
    gfx_text_c(140, T("ANY KEY: MENU", "BÁRMELY GOMB: MENÜ"), 0);
    ui_wait_key();
}

void play_session(uint8_t game, uint8_t resume) {
    uint8_t go, ev;
    if (!resume || !active) {
        if (active) record_result();                              /* an unfinished session is scored when it is replaced */
        cur_game = game;
        rng_seed((uint16_t)(rng_next() ^ (uint16_t)(hw_frames() * 2749u) ^ 1u));
        tbl_init(&TB, game, &hooks);
        pick_names();
        active = 1; over = 0;
    }
    gfx_clear();
    view_reset();
    input_wait_release();
    for (;;) {
        TB.quit = 0;
        go = tbl_hand(&TB);
        if (TB.quit) { view_update(&TB); break; }                  /* the hand was played out; back to the menu, the session stays */
        view_update(&TB);
        if (!go) { active = 0; end_screen(); return; }
        status(T("SPACE: NEXT HAND  M: MENU", "SZÓKÖZ: KÖVETKEZŐ  M: MENÜ"));
        input_wait_release();
        for (;;) {
            hw_wait_frame();
            ev = input_poll();
            if (ev == KEY_SELECT || ev == KEY_CALL) break;
            if (ev == KEY_SOUND) toggle_sound();
            if (ev == KEY_PAUSE) { do_pause(); view_update(&TB); }
            if ((ev == KEY_BACK || ev == KEY_MENU) && ask_leave()) return;
            if (ev == KEY_BACK || ev == KEY_MENU) status(T("SPACE: NEXT HAND  M: MENU", "SZÓKÖZ: KÖVETKEZŐ  M: MENÜ"));
        }
    }
    (void)over;
}
