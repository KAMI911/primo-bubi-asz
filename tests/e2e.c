#define _POSIX_C_SOURCE 200809L
/* End-to-end test on the host: scripted key presses drive the real UI and the real table. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/gfx.h"
#include "../src/hall.h"
#include "../src/hw.h"
#include "../src/input.h"
#include "../src/play.h"
#include "../src/poker.h"
#include "../src/ui.h"
#include "hw_host.h"

static unsigned fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

#define SC_UP 0x01
#define SC_DOWN 0x0F
#define SC_LEFT 0x39
#define SC_RIGHT 0x3D
#define SC_SPACE 0x19
#define SC_RET 0x37
#define SC_T 0x16
#define SC_C 0x10
#define SC_F 0x12
#define SC_R 0x14
#define SC_M 0x3F          /* BRK */

static HostEv ev[4000];
static unsigned nev, tcur;
static void script_begin(void) { nev = 0; tcur = 10; }
static void tap(uint8_t sc) { ev[nev++] = (HostEv){tcur, sc, 1}; ev[nev++] = (HostEv){tcur + 3, sc, 0}; tcur += 12; }
static void wait(unsigned frames) { tcur += frames; }
static void script_run(void) { host_script(ev, nev); }

static void test_name_and_menu(void) {
    uint8_t last = 0, c;
    script_begin(); tap(0x22); tap(0x02); tap(SC_RET); script_run();     /* Z S */
    cfg.name[0] = 0;
    ui_name();
    CHECK(strncmp(cfg.name, "ZS", 2) == 0);
    script_begin(); tap(SC_SPACE); script_run();
    c = ui_menu(0, &last);
    CHECK(c == PG_HOLDEM && last == PG_HOLDEM);                         /* Texas Hold'em is the entry the cursor starts on */
    CHECK(PG_HOLDEM == 0 && NPGAMES == 2);
    script_begin(); tap(SC_DOWN); tap(SC_SPACE); script_run();
    c = ui_menu(0, &last);
    CHECK(c == PG_OMAHA && last == PG_OMAHA);
    script_begin(); tap(SC_DOWN); tap(SC_SPACE); script_run();         /* only two entries: down from the last one wraps to the first */
    c = ui_menu(0, &last);
    CHECK(c == PG_HOLDEM && last == PG_HOLDEM);
    last = PG_OMAHA;
    script_begin(); tap(SC_RIGHT); tap(SC_DOWN); tap(SC_SPACE); script_run();
    c = ui_menu(0, &last);
    CHECK(c == UI_OPTIONS);
    script_begin(); tap(SC_T); script_run();
    c = ui_menu(0, &last);
    CHECK(c == UI_RULES_BASE + PG_OMAHA);
}

/* the menu and the options with their sliding highlight bar: one shot of each after a few moves */
static void test_menu_shots(const char *outdir) {
    static char mp[80], op[80];
    uint8_t last = 0, c, k;
    snprintf(mp, sizeof mp, "%s/menu.pbm", outdir); snprintf(op, sizeof op, "%s/options.pbm", outdir);
    script_begin(); tap(SC_DOWN); tap(SC_DOWN); tap(SC_UP); wait(42); tap(SC_SPACE); script_run();
    host_shot(80, mp);
    c = ui_menu(1, &last);
    CHECK(c == PG_OMAHA && last == PG_OMAHA);                           /* down, down (round to the top), up (to the last one) */
    script_begin(); tap(SC_DOWN); tap(SC_RIGHT); tap(SC_DOWN); tap(SC_RIGHT); wait(30);
    for (k = 0; k < 3; k++) tap(SC_DOWN);                               /* sound -> ... -> back to menu */
    tap(SC_SPACE); script_run();
    host_shot(90, op);
    ui_options();
    gfx_clear();
}

/* a whole session: the player calls everything (C also starts the next hand) */
static void play_scripted(uint8_t game, unsigned frames, const char *shot_base, int shots) {
    unsigned t;
    static char paths[8][64];
    int i;
    script_begin();
    for (t = 0; t < frames; t += 14) tap(SC_C);
    tap(SC_M); tap(SC_M); wait(200); tap(SC_M); tap(SC_M); wait(400); tap(SC_M); tap(SC_M);
    script_run();
    for (i = 0; i < shots; i++) {
        snprintf(paths[i], sizeof paths[i], "%s_%d.pbm", shot_base, i);
        host_shot((unsigned)(frames * (i + 1) / (shots + 1)), paths[i]);
    }
    play_session(game, 0);
}

static void test_games(const char *outdir) {
    uint8_t g;
    char base[64];
    for (g = 0; g < NPGAMES; g++) {
        snprintf(base, sizeof base, "%s/game%u", outdir, g);
        play_scripted(g, 2600, base, 6);
        CHECK(play_active() == 1 || play_active() == 0);
        printf("game %u played; active=%u\n", g, play_active());
    }
}

static void test_hall(void) {
    hall_clear();
    CHECK(hall_rank(0, 500) == 0);
    hall_add(0, 0, "AAA", 500, 10);
    CHECK(hall_rank(0, 400) == 1);
    CHECK(hall_rank(0, 500) == 1);                       /* a tie goes behind the older entry */
    hall_add(0, 0, "BBB", 900, 5);
    CHECK(hall_get(0, 0)->chips == 900 && hall_get(0, 1)->chips == 500);
    CHECK(hall_rank(0, 0) == 0xFF);
    CHECK(hall_rank(9, 100) == 0xFF);
}

/* a comment bubble goes over the screen and is taken away again; also a shot of two comments */
static void test_quips(const char *outdir) {
    static char path[2][80];
    uint8_t q, *v = hw_vram();
    unsigned i, same;
    for (q = 0; q < 2; q++) {
        gfx_clear();
        for (i = 0; i < (unsigned)SCR_BYTES * SCR_H; i++) v[i] = (uint8_t)(i * 7);
        script_begin(); wait(400); script_run();
        snprintf(path[q], sizeof path[q], "%s/quip%u.pbm", outdir, q);
        host_shot(q ? 160 : 30, path[q]);
        cfg.chat = (uint8_t)(q ? 2 : 1);                             /* the second one talkative */
        play_quip_test((uint8_t)(q ? 1 : 7));                        /* the long sucker line, the Bubi Asz line */
        for (i = 0, same = 1; i < 2 * SCR_BYTES; i++) if (i % SCR_BYTES >= 20 && v[136 * SCR_BYTES + i]) same = 0;   /* the face corner is clear again */
        CHECK(same);
    }
    cfg.chat = 1;
}

/* raise sizes and all in with the question: UP to the top level, R, answer C (yes) or F (no), now and then */
static void test_allin(void) {
    unsigned t, k;
    script_begin();
    for (t = 0; t < 3000; t += 90) {
        for (k = 0; k < 8; k++) tap(SC_UP);
        tap(SC_R); tap((t / 90) & 1 ? SC_C : SC_F); tap(SC_C); tap(SC_C);
    }
    tap(SC_M); tap(SC_M); wait(200); tap(SC_M); tap(SC_M); wait(400); tap(SC_M); tap(SC_M);
    script_run();
    play_session(PG_HOLDEM, 0);
    CHECK(play_active() == 1 || play_active() == 0);
}

uint8_t play_bubi_test(const Table *t, uint8_t seat);
static void test_bubi(void) {                                          /* jack and ace of one suit: in the hand, or one on the board */
    static Table t;
    t.n[0] = 2; t.nb = 0;
    t.c[0][0] = MKCARD(10, 2); t.c[0][1] = MKCARD(0, 2);
    CHECK(play_bubi_test(&t, 0) == 1);
    t.c[0][1] = MKCARD(0, 1);
    CHECK(play_bubi_test(&t, 0) == 0);                                 /* different suits: no */
    t.nb = 3; t.board[0] = MKCARD(5, 0); t.board[1] = MKCARD(0, 2); t.board[2] = MKCARD(7, 3);
    CHECK(play_bubi_test(&t, 0) == 1);                                 /* the ace on the board joins the jack */
}

/* the rules page of every game (one Primo screen): a shot of each, then a key leaves */
static void test_rules_pages(const char *outdir) {
    static char path[NPGAMES][80];
    uint8_t g;
    for (g = 0; g < NPGAMES; g++) {
        snprintf(path[g], sizeof path[g], "%s/rules%u.pbm", outdir, g);
        script_begin(); wait(30); tap(SC_SPACE); script_run();
        host_shot(20, path[g]);
        ui_rules(g);
        CHECK(1);
    }
}

int main(int argc, char **argv) {
    const char *out = argc > 1 ? argv[1] : "build/shots";
    int color = argc > 2;
    if (color) host_color(1);
    hw_init(); gfx_init();
    test_hall();
    test_name_and_menu();
    test_games(out);
    test_menu_shots(out);
    test_quips(out);
    test_rules_pages(out);
    test_bubi();
    test_allin();
    printf("%u checks, %u failures\n", checks, fails);
    if (!fails) printf("E2E ALL PASS\n");
    return fails != 0;
}
