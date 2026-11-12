#include "play.h"
#include "ui.h"
#include "gfx.h"
#include "layout.h"

#define ROW_Y(i) ((uint8_t)(L_MENU_ROW0_Y + L_MENU_STEP * (i)))
#define BAR_Y(i) ((uint8_t)(ROW_Y(i) + L_MENU_BAR_DY))
#include "hall.h"
#include "hw.h"
#include "input.h"
#include "poker.h"
#include "ptext.h"

/* the TABLE row (colour model) exists in the colour tape always, in the monochrome tape never, in host builds by model */
#if defined(TARGET_BW)
#define HAS_TABLE 0
#elif defined(TARGET_COLOR)
#define HAS_TABLE 1
#else
#define HAS_TABLE hw_is_color()
#endif

Settings cfg = {0, 0, 1, 0, 0, 0, 1, {0}};

/* ---- small helpers ----------------------------------------------------------------------------- */
void ui_snd(uint8_t kind) {
    if (!cfg.sound) return;
    switch (kind) {
    case 0: hw_beep(20, 2); break;
    case 1: hw_beep(110, 6); break;
    case 2: hw_beep(30, 3); hw_beep(18, 3); break;
    default: hw_beep(40, 4); hw_beep(32, 4); hw_beep(26, 4); hw_beep(20, 10); break;
    }
}

static uint8_t next_event(void) {
    uint8_t e;
    do { hw_wait_frame(); e = input_poll(); } while (e == KEY_NONE);
    return e;
}

/* any key leaves */
void ui_wait_key(void) {
    input_wait_release();
    do hw_wait_frame(); while (input_poll() == KEY_NONE && !input_any_down());
    input_wait_release();
}

static void header(const char *title) {
    gfx_clear();
    gfx_text_c(L_HEADER_Y, title, 1);
}

static void pad_copy(char *dst, const char *src, uint8_t width) {         /* left aligned, space padded, terminated */
    uint8_t i;
    for (i = 0; i < width && src[i]; i++) dst[i] = src[i];
    for (; i < width; i++) dst[i] = ' ';
    dst[width] = 0;
}

static const char *const game_names[NPGAMES] = {            /* the order of the PG_ enum: the first one is the default */
    T("TEXAS HOLD'EM", "TEXAS HOLD'EM"), T("OMAHA", "OMAHA")
};
const char *ui_game_name(uint8_t game) { return game_names[game % NPGAMES]; }

/* ---- title --------------------------------------------------------------------------------------- */
void ui_title(void) {
    uint8_t i, blink = 0;
    static const card_t royal[5] = {MKCARD(0, 0) | C_UP, MKCARD(12, 0) | C_UP, MKCARD(11, 0) | C_UP, MKCARD(10, 0) | C_UP, MKCARD(9, 0) | C_UP};
    gfx_clear();
    gfx_text2(L_TITLE_BIG_X, L_TITLE_BIG_Y, T("BUBI ASZ", "BUBI ÁSZ"));
    gfx_text_c(L_TITLE_SUB_Y, T("POKER FOR THE PRIMO", "POKER A PRIMÓRA"), 0);
    for (i = 0; i < 5; i++) gfx_card((uint8_t)(L_TITLE_CARDS_X + i * L_TITLE_CARDS_STEP), L_TITLE_CARDS_Y, royal[i], 0);
    gfx_text_c(L_TITLE_MODEL_Y, hw_is_color() ? T("COLOUR PRIMO", "SZÍNES PRIMO") : T("MONOCHROME PRIMO", "FEKETE-FEHÉR PRIMO"), 0);
    gfx_title_colours(1);
    gfx_credit(L_TITLE_MADE_Y, T("MADE BY:", "KÉSZÍTETTE:"));     /* the title texts (Hungarian on the Hungarian tapes); KAMI in four colours */
    gfx_text_c(L_TITLE_COFFEE_Y, T("BUY ME A COFFEE:", "HÍVJ MEG EGY KÁVÉRA:"), 0);
    gfx_text_c(L_TITLE_KOFI_Y, "HTTPS://KO-FI.COM/KAMI911", 0);
    gfx_text_c(L_TITLE_NEWS_Y, T("NEWS AND TIPS:", "HÍREK, TIPPEK:"), 0);
    gfx_text_c(L_TITLE_LINUX_Y, "HTTPS://LINUXMINT.HU", 0);
    input_wait_release();
    for (;;) {
        gfx_text_c(L_TITLE_KEY_Y, T("PRESS ANY KEY", "NYOMJ MEG EGY GOMBOT"), (uint8_t)(blink >> 4 & 1));
        blink++;
        hw_wait_frame();
        if (input_poll() != KEY_NONE || input_any_down()) break;
    }
    gfx_title_colours(0);                                          /* the commentators' colours come back */
    input_wait_release();
}

/* ---- name entry ---------------------------------------------------------------------------------- */
static char next_char(char c, int8_t d) {                  /* cycles: space, A-Z, 0-9 */
    static const char set[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    uint8_t i = 0;
    while (set[i] && set[i] != c) i++;
    if (!set[i]) i = 0;
    i = (uint8_t)((i + 37 + d) % 37);
    return set[i];
}

void ui_name(void) {
    char buf[HALL_NAME + 1] = {0};
    uint8_t pos = 0, len = 0, i, ev, redraw = 1;
    char line[HALL_NAME + 3], shown[HALL_NAME];
    for (i = 0; cfg.name[i] && i < HALL_NAME; i++) buf[i] = cfg.name[i];
    len = i; pos = len < HALL_NAME ? len : (uint8_t)(HALL_NAME - 1);
    header(T("YOUR NAME", "A NEVED"));
    gfx_text_c(L_NAME_HELP_Y, T("TYPE LETTERS OR USE UP/DOWN", "ÍRD BE, VAGY FEL/LE NYILAK"), 0);
    gfx_text_c(L_NAME_HELP_Y + L_NAME_HELP_STEP, T("LEFT/RIGHT MOVE  BACKSPACE", "BAL/JOBB MOZGAT  TÖRLÉS"), 0);
    gfx_text_c(L_NAME_HELP_Y + 2 * L_NAME_HELP_STEP, T("SPACE/RETURN = DONE", "SZÓKÖZ/RETURN = KÉSZ"), 0);
    input_wait_release();
    input_set_text(1);
    for (i = 0; i < HALL_NAME; i++) shown[i] = 0;
    for (;;) {
        if (redraw) {
            for (i = 0; i < HALL_NAME; i++) line[i] = (i < len || i == pos) ? (buf[i] ? buf[i] : ' ') : ' ';
            line[HALL_NAME] = 0;
            for (i = 0; i < HALL_NAME; i++)                         /* only the letters that changed */
                if (line[i] != shown[i]) { char t[2]; t[0] = shown[i] = line[i]; t[1] = 0; gfx_text2((uint8_t)(L_NAME_X + 2 * i), L_NAME_Y, t); }
            redraw = 0;
        }
        gfx_hline(L_NAME_CURSOR_Y, (uint8_t)(L_NAME_CURSOR_X + 4 * pos), 4);
        for (i = 0; i < HALL_NAME; i++) if (i != pos) gfx_fill((uint8_t)(L_NAME_CURSOR_X + 4 * i), L_NAME_CURSOR_Y, 4, 1, 0);
        hw_wait_frame();
        ev = input_poll();
        if (ev == KEY_NONE) continue;
        if (ev == KEY_CHAR) {
            if (input_char() == ' ' && len == 0) continue;
            buf[pos] = input_char(); if (pos >= len) len = (uint8_t)(pos + 1);
            if (pos + 1 < HALL_NAME) pos++;
            redraw = 1; ui_snd(0);
        } else if (ev == KEY_UP || ev == KEY_DOWN) {
            buf[pos] = next_char(buf[pos] ? buf[pos] : ' ', ev == KEY_UP ? 1 : -1);
            if (pos >= len) len = (uint8_t)(pos + 1);
            redraw = 1;
        } else if (ev == KEY_RIGHT) {
            if (pos + 1 < HALL_NAME && pos < len) pos++;
            else if (pos + 1 < HALL_NAME) { buf[pos] = buf[pos] ? buf[pos] : 'A'; if (pos >= len) len = (uint8_t)(pos + 1); pos++; }
            redraw = 1;
        } else if (ev == KEY_LEFT || ev == KEY_BKSP) {
            if (ev == KEY_BKSP || pos == 0) { if (pos > 0) { pos--; } buf[pos] = 0; if (len > pos) len = pos; }
            else pos--;
            redraw = 1;
        } else if (ev == KEY_SELECT || ev == KEY_BACK) {
            while (len && (buf[len - 1] == ' ' || !buf[len - 1])) len--;
            if (len == 0) { buf[0] = 'P'; buf[1] = 'L'; buf[2] = 'A'; buf[3] = 'Y'; buf[4] = 'E'; buf[5] = 'R'; len = 6; }
            for (i = 0; i < HALL_NAME + 1; i++) cfg.name[i] = i < len ? (buf[i] ? buf[i] : ' ') : 0;
            break;
        }
    }
    input_set_text(0);
    input_wait_release();
}

/* ---- menu ---------------------------------------------------------------------------------------- */
static const char *const menu_side[6] = {
    T("CONTINUE", "FOLYTATÁS"), T("HALL OF FAME", "DICSŐSÉG"), T("OPTIONS", "BEÁLLÍTÁS"), T("HELP", "SÚGÓ"), T("CHANGE NAME", "NÉVCSERE"), T("QUIT", "KILÉPÉS")
};

/* one menu entry on a cleared screen; the highlight is a separate XOR bar (menu_bar), so moving the cursor only touches the bar */
static void menu_item(uint8_t col, uint8_t i, uint8_t can_continue) {
    char line[20];
    if (col == 0) {
        pad_copy(line, game_names[i], 15);
        gfx_text(L_MENU_GAMES_X, (uint8_t)(ROW_Y(i + L_MENU_GAMES_DOWN + L_MENU_DOWN)), line, 0);     /* the games start lower: level with HALL OF FAME when there is nothing to continue */
    } else if (i == 0 && !can_continue) {
        gfx_text(L_MENU_SIDE_X, ROW_Y(L_MENU_DOWN), "              ", 0);
    } else {
        pad_copy(line, menu_side[i], 14);
        gfx_text(L_MENU_SIDE_X, ROW_Y(i + L_MENU_DOWN), line, 0);
    }
}

/* the highlight is a bar of 11 lines over the text of an entry (text at ROW_Y(i), bar from 2 lines above); rows are 12 lines apart */

static uint8_t bar_x, bar_w;                                   /* the bar's column and width in 4-pixel units */
static void bar(uint8_t y, uint8_t h) { gfx_xor_rect(bar_x, y, bar_w, h); }
static void bar_col(uint8_t col) { bar_x = col ? L_MENU_SBAR_X : L_MENU_GBAR_X; bar_w = col ? L_MENU_SBAR_W : L_MENU_GBAR_W; }

/* moves the highlight bar from y0 to y1: to the next row it slides in 4-pixel steps, inverting only the strips it leaves and
   enters, so nothing flashes; to any other row it jumps */
static void bar_move(uint8_t y0, uint8_t y1) {
    uint8_t n, k = y0 < y1 ? y0 : y1;                          /* the upper of the two positions */
    if ((uint8_t)(y1 - y0) != L_MENU_STEP && (uint8_t)(y0 - y1) != L_MENU_STEP) { bar(y0, L_MENU_BAR_H); bar(y1, L_MENU_BAR_H); return; }
    for (n = 0; n < L_MENU_STEP; n += 4) {                              /* moving up shows the same strips in the opposite order */
        uint8_t m = y0 < y1 ? n : (uint8_t)(L_MENU_STEP - 4 - n);
        bar((uint8_t)(k + m), 4);                              /* the strip the bar leaves (down) or enters (up) ... */
        bar((uint8_t)(k + m + L_MENU_BAR_H), 4);                         /* ... and the other one */
        hw_wait_frame();
    }
}

uint8_t ui_menu(uint8_t can_continue, uint8_t *last_game) {
    uint8_t col = 0, row = *last_game < NPGAMES ? *last_game : 0, srow = 0, i, ev;
    uint8_t pcol, prow, psrow;
    input_wait_release();
    gfx_clear();                                              /* the whole menu once, then only the entries that change */
    gfx_text2(L_MENU_TITLE_X, L_MENU_TITLE_Y, T("BUBI ASZ", "BUBI ÁSZ"));
    for (i = 0; i < NPGAMES; i++) menu_item(0, i, can_continue);
    for (i = 0; i < 6; i++) menu_item(1, i, can_continue);
    gfx_text(L_MENU_SIDE_X, L_MENU_PLAYER_Y, T("PLAYER:", "JÁTÉKOS:"), 0);
    gfx_text(L_MENU_SIDE_X, L_MENU_NAME_Y, cfg.name, 0);
    gfx_text(L_MENU_GAMES_X, L_MENU_HELP_Y, T("ARROWS OR IJKL", "NYILAK VAGY IJKL"), 0);
    gfx_text(L_MENU_GAMES_X, L_MENU_RULES_Y, T("T: RULES", "T: SZABÁLYOK"), 0);
    bar_col(0); bar(BAR_Y(row + L_MENU_GAMES_DOWN + L_MENU_DOWN), L_MENU_BAR_H);
    for (;;) {
        pcol = col; prow = row; psrow = srow;
        ev = next_event();
        if (ev == KEY_UP)   { if (col == 0) row = (uint8_t)((row + NPGAMES - 1) % NPGAMES); else { srow = (uint8_t)((srow + 5) % 6); if (srow == 0 && !can_continue) srow = 5; } }
        else if (ev == KEY_DOWN) { if (col == 0) row = (uint8_t)((row + 1) % NPGAMES); else { srow = (uint8_t)((srow + 1) % 6); if (srow == 0 && !can_continue) srow = 1; } }
        else if (ev == KEY_LEFT || ev == KEY_RIGHT) { col = (uint8_t)(1 - col); if (col == 1 && srow == 0 && !can_continue) srow = 1; }
        else if (ev == KEY_HINT) { *last_game = row; return (uint8_t)(UI_RULES_BASE + row); }
        else if (ev == KEY_SELECT) {
            ui_snd(0);
            if (col == 0) { *last_game = row; return row; }
            return (uint8_t)(UI_CONTINUE + srow);
        }
        else if (ev == KEY_CHAR && input_char() == 'Q') return UI_QUIT;
        else if (ev == KEY_BACK && can_continue) return UI_CONTINUE;
        if (pcol != col || prow != row || psrow != srow) {      /* un-highlight the old entry, highlight the new one */
            uint8_t i0 = pcol == 0 ? (uint8_t)(prow + L_MENU_GAMES_DOWN + L_MENU_DOWN) : (uint8_t)(psrow + L_MENU_DOWN), i1 = col == 0 ? (uint8_t)(row + L_MENU_GAMES_DOWN + L_MENU_DOWN) : (uint8_t)(srow + L_MENU_DOWN);   /* the game column is one row down */
            bar_col(pcol);
            if (pcol == col) bar_move(BAR_Y(i0), BAR_Y(i1));
            else { bar(BAR_Y(i0), L_MENU_BAR_H); bar_col(col); bar(BAR_Y(i1), L_MENU_BAR_H); }
        }
    }
}


static const char *onoff(uint8_t v) { return v ? T("ON", "BE") : T("OFF", "KI"); }

/* ---- options ------------------------------------------------------------------------------------- */
enum { O_BACK, O_CTRL, O_SOUND, O_SCREEN, O_CHAT, O_TABLE, O_EXIT };
static uint8_t opt_id(uint8_t i) { return (uint8_t)(!HAS_TABLE && i >= O_TABLE ? O_EXIT : i); }   /* the setting shown at position i */

/* one row; the selected row shows its value between < and >, the arrows say that LEFT/RIGHT changes it */
static void options_row(uint8_t i, uint8_t sel) {
    const char *a, *b = "";
    char line[33], bk[12];
    uint8_t id = opt_id(i), k = 13;
    switch (id) {
    case O_BACK: {                                              /* the design and (colour model) the colour of the card back in one row */
        a = T("CARD BACK", "LAPHÁTLAP");
        k = 0;
        if (HAS_TABLE) {
            const char *cn = cfg.bcol == 1 ? T("RED", "PIROS") : cfg.bcol == 2 ? T("GREEN", "ZÖLD") : T("BLUE", "KÉK");
            while (*cn) bk[k++] = *cn++;
            bk[k++] = ' ';
        }
        k = (uint8_t)(k + gfx_u16(&bk[k], (uint16_t)(cfg.back + 1)));
        bk[k] = 0; b = bk; k = 13;
        break; }
    case O_CTRL: a = T("CONTROLS", "IRÁNYÍTÁS"); b = input_scheme_name(cfg.scheme); break;
    case O_SOUND: a = T("SOUND", "HANG"); b = onoff(cfg.sound); break;
    case O_SCREEN: a = T("SCREEN", "KÉP"); b = cfg.invert ? T("INVERSE", "FORDÍTOTT") : T("NORMAL", "NORMÁL"); break;
    case O_CHAT: a = T("COMMENTATORS", "KOMMENTÁTOR"); b = cfg.chat == 2 ? T("DRUNK", "RÉSZEG") : onoff(cfg.chat); break;
    case O_TABLE: a = T("TABLE", "ASZTAL"); b = cfg.table == 1 ? T("BURGUNDY", "BORDÓ") : cfg.table == 2 ? T("BLACK", "FEKETE") : T("GREEN", "ZÖLD"); break;
    default: a = T("BACK TO MENU", "VISSZA"); id = 0xFF; break;
    }
    pad_copy(line, a, 13);
    if (id != 0xFF) {
        for (; k < 32; k++) line[k] = ' ';
        line[32] = 0;
        if (sel) line[13] = '<';
        for (k = 15; *b && k < 29; k++, b++) line[k] = *b;
        if (sel) line[k + 1] = '>';
    }
    gfx_text(L_OPT_X, ROW_Y(i), line, 0);
}

static void options_backs(void) {
    gfx_card(L_OPT_CARDS_X, L_OPT_CARDS_Y, (card_t)0, cfg.back);                   /* current card back (face-down) */
    gfx_card(L_OPT_CARDS_X2, L_OPT_CARDS_Y, (card_t)(MKCARD(0, 0) | C_UP), cfg.back);
}

void ui_options(void) {
    uint8_t sel = 0, ev, i, old, full = 1, nrow = HAS_TABLE ? 7 : 6;
    input_wait_release();
    for (;;) {
        if (full) {                                           /* everything once; afterwards only what a key changes */
            header(T("OPTIONS", "BEÁLLÍTÁSOK"));
            for (i = 0; i < nrow; i++) options_row(i, (uint8_t)(i == sel));
            options_backs();
            gfx_text(L_OPT_X, (uint8_t)(ROW_Y(nrow) + L_OPT_HINT_DY), T("LEFT/RIGHT CHANGES", "BAL/JOBB VÁLTOZTAT"), 0);
            bar_x = L_OPT_BAR_X; bar_w = L_OPT_BAR_W; bar(BAR_Y(sel), L_MENU_BAR_H);
            full = 0;
        }
        old = sel;
        ev = next_event();
        if (ev == KEY_UP) sel = (uint8_t)((sel + nrow - 1) % nrow);
        else if (ev == KEY_DOWN) sel = (uint8_t)((sel + 1) % nrow);
        else if (ev == KEY_LEFT || ev == KEY_RIGHT || ev == KEY_SELECT) {
            int8_t d = (ev == KEY_LEFT) ? -1 : 1;
            uint8_t id = opt_id(sel);
            if (id == O_BACK) {                                   /* every design in every colour, one after the other */
                int8_t n = (int8_t)(cfg.back + d);
                if (n < 0 || n >= NBACKS) {
                    if (HAS_TABLE) { cfg.bcol = (uint8_t)((cfg.bcol + 3 + d) % 3); gfx_set_back_colour(cfg.bcol); }
                    n = (int8_t)(n < 0 ? NBACKS - 1 : 0);
                }
                cfg.back = (uint8_t)n; options_backs();
            }
            else if (id == O_CTRL) { cfg.scheme = (uint8_t)((cfg.scheme + NSCHEMES + d) % NSCHEMES); input_set_scheme(cfg.scheme); }
            else if (id == O_SOUND) cfg.sound = (uint8_t)!cfg.sound;
            else if (id == O_SCREEN) { cfg.invert = (uint8_t)!cfg.invert; gfx_set_invert(cfg.invert); full = 1; }
            else if (id == O_CHAT) cfg.chat = (uint8_t)((cfg.chat + 3 + d) % 3);
            else if (id == O_TABLE) { cfg.table = (uint8_t)((cfg.table + 3 + d) % 3); gfx_set_table(cfg.table); }
            else if (ev == KEY_SELECT) break;
            ui_snd(0);
        }
        else if (ev == KEY_BACK) break;
        if (!full) {
            if (old != sel) { bar(BAR_Y(old), L_MENU_BAR_H); options_row(old, 0); options_row(sel, 1); bar(BAR_Y(sel), L_MENU_BAR_H); }   /* the arrows move with the selection */
            else { bar(BAR_Y(sel), L_MENU_BAR_H); options_row(sel, 1); bar(BAR_Y(sel), L_MENU_BAR_H); }   /* a changed value: bar off, text, bar on */
        }
    }
    input_wait_release();
}

/* ---- hall of fame -------------------------------------------------------------------------------- */
void ui_hall(uint8_t game) {
    uint8_t i, ev, k;
    char line[33];
    input_wait_release();
    for (;;) {
        header(T("HALL OF FAME", "DICSŐSÉGLISTA"));
        pad_copy(line, game_names[game], 16);
        gfx_text_c(L_HALL_GAME_Y, line, 0);
        for (i = 0; i < HALL_ENTRIES; i++) {
            const HallRec *r = hall_get(game, i);
            pad_copy(line, "", 32);
            line[0] = (char)('1' + i); line[1] = '.';
            if (!r->chips) { line[3] = '-'; }
            else {
                for (k = 0; k < HALL_NAME && r->name[k]; k++) line[3 + k] = r->name[k];
                k = gfx_u16(&line[13], r->chips);
                line[13 + k] = ' '; line[14 + k] = '/'; k = (uint8_t)(k + gfx_u16(&line[15 + k], r->hands)); line[15 + k] = 0;
            }
            gfx_text(0, (uint8_t)(L_HALL_ROW0_Y + L_HALL_STEP * i), line, 0);
        }
        gfx_text_c(L_HALL_CAPTION_Y, T("CHIPS / HANDS PLAYED", "ZSETON / OSZTÁS"), 0);
        gfx_text_c(L_HALL_HINT1_Y, T("LEFT/RIGHT OTHER GAME", "BAL/JOBB MÁS JÁTÉK"), 0);
        gfx_text_c(L_HALL_HINT2_Y, T("SPACE OR BRK BACK", "SZÓKÖZ VAGY BRK: VISSZA"), 0);
        ev = next_event();
        if (ev == KEY_LEFT) game = (uint8_t)((game + NPGAMES - 1) % NPGAMES);
        else if (ev == KEY_RIGHT) game = (uint8_t)((game + 1) % NPGAMES);
        else if (ev == KEY_SELECT || ev == KEY_BACK) break;
    }
    input_wait_release();
}

/* ---- rules and help --------------------------------------------------------------------------------- */
void ui_rules(uint8_t game) {
    const char *p;
    uint8_t k, y = L_RULES_Y0;
    header(game_names[game % NPGAMES]);
    for (k = 0; (p = play_rules_line((uint8_t)(game % NPGAMES), k)) != 0; k++, y = (uint8_t)(y + L_RULES_STEP)) gfx_text(0, y, p, 0);
    for (k = 0, y = L_RULES_COMMON_Y; (p = play_rules_line(NPGAMES, k)) != 0; k++, y = (uint8_t)(y + L_RULES_STEP)) gfx_text(0, y, p, 0);
    gfx_text_c(L_FOOTER_Y, T("ANY KEY: BACK", "BÁRMELY GOMB: VISSZA"), 1);
    ui_wait_key();
}

void ui_help(void) {
    header(T("HELP", "SÚGÓ"));
    gfx_text(0, L_HELP_Y0, T("ON YOUR TURN:", "A TE LÉPÉSEDNÉL:"), 0);
    gfx_text(0, L_HELP_Y1, T("F FOLD  C CALL/CHECK  R RAISE", "F ELDOB C MEGAD/PASSZ R EMEL"), 0);
    gfx_text(0, L_HELP_Y2, T("UP/DOWN: RAISE SIZE, ALL IN", "FEL/LE: EMELÉS, ALL IN"), 0);
    gfx_text(0, L_HELP_Y3, T("T: CHANCE  M/BRK: MENU  Z PAUSE", "T: ESÉLY  M/BRK: MENÜ  Z SZÜNET"), 0);
    gfx_text_c(L_FOOTER_Y, T("ANY KEY: BACK", "BÁRMELY GOMB: VISSZA"), 1);
    ui_wait_key();
}
