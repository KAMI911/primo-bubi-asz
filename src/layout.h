#ifndef LAYOUT_H
#define LAYOUT_H
/* Where everything is on the screen. Pure compile-time constants (no run-time cost): change a number here and rebuild.
   Units: y is a pixel line (0..191, a text line is 8 lines tall); x is either a text column (4 pixels, 0..63: gfx_text),
   a byte column (8 pixels, 0..31: gfx_text2 and the widths of the cached fields) or a card column (x4, 4 pixels: gfx_card), as noted. */

/* ---- every screen: the title bar (centred, inverse) and the "any key" line at the bottom ---- */
#define L_HEADER_Y        2
#define L_FOOTER_Y        184

/* ---- title screen ---- */
#define L_TITLE_BIG_X     8       /* big name, byte column */
#define L_TITLE_BIG_Y     18
#define L_TITLE_SUB_Y     44
#define L_TITLE_CARDS_X   11      /* the five royal flush cards, card column and step */
#define L_TITLE_CARDS_STEP 9
#define L_TITLE_CARDS_Y   68
#define L_TITLE_MODEL_Y   104
#define L_TITLE_KEY_Y     118     /* the blinking "press any key" */
#define L_TITLE_MADE_Y    134
#define L_TITLE_COFFEE_Y  150
#define L_TITLE_KOFI_Y    160
#define L_TITLE_NEWS_Y    172
#define L_TITLE_LINUX_Y   182

/* ---- name entry ---- */
#define L_NAME_HELP_Y     40      /* three help lines, 12 lines apart */
#define L_NAME_HELP_STEP  12
#define L_NAME_X          8       /* the name in big letters: byte column of the first one (2 per letter) and y */
#define L_NAME_Y          96
#define L_NAME_CURSOR_X   16      /* the underline: card column (4 pixels) of the first letter, 4 per letter */
#define L_NAME_CURSOR_Y   114

/* ---- main menu ---- */
#define L_MENU_TITLE_X    8       /* big name, byte column */
#define L_MENU_TITLE_Y    4
#define L_MENU_ROW0_Y     24      /* first row of the right column; rows are L_MENU_STEP apart (also used by the options) */
#define L_MENU_STEP       12      /* must be a multiple of 4: the highlight bar slides in 4-pixel steps */
#define L_MENU_DOWN       1       /* the whole menu (both columns and their bar) starts this many rows lower than the options rows */
#define L_MENU_GAMES_DOWN 1       /* the games start this many rows lower than the right column */
#define L_MENU_GAMES_X    0       /* text columns */
#define L_MENU_SIDE_X     17
#define L_MENU_BAR_DY     (-2)    /* the highlight bar starts this far above the text, is L_MENU_BAR_H lines tall */
#define L_MENU_BAR_H      11
#define L_MENU_GBAR_X     0       /* its columns (4 pixels) and width: over the games ... */
#define L_MENU_GBAR_W     30
#define L_MENU_SBAR_X     34      /* ... and over the right column */
#define L_MENU_SBAR_W     28
#define L_MENU_PLAYER_Y   112
#define L_MENU_NAME_Y     122
#define L_MENU_HELP_Y     120     /* "ARROWS OR IJKL" */
#define L_MENU_RULES_Y    130

/* ---- options ---- */
#define L_OPT_X           2       /* text column of the labels */
#define L_OPT_VALUE_COL   13      /* the value starts this many characters after it (the < arrow is the first of them) */
#define L_OPT_BAR_X       4
#define L_OPT_BAR_W       60
#define L_OPT_HINT_DY     8       /* the "LEFT/RIGHT CHANGES" line is this far below the last row */
#define L_OPT_CARDS_X     46      /* the two sample cards: card columns, y */
#define L_OPT_CARDS_X2    54
#define L_OPT_CARDS_Y     144

/* ---- hall of fame ---- */
#define L_HALL_GAME_Y     22
#define L_HALL_ROW0_Y     44
#define L_HALL_STEP       14
#define L_HALL_CAPTION_Y  128
#define L_HALL_HINT1_Y    150
#define L_HALL_HINT2_Y    162

/* ---- rules and help ---- */
#define L_RULES_Y0        28      /* the game's own rules; lines are L_RULES_STEP apart */
#define L_RULES_COMMON_Y  100     /* the rules every game shares */
#define L_RULES_STEP      10
#define L_HELP_Y0         66      /* the help lines (y of each) */
#define L_HELP_Y1         76
#define L_HELP_Y2         94
#define L_HELP_Y3         104

/* ---- the table (play.c) ---- */
#define Y_TITLE   0
#define Y_AI      12            /* three text lines of an opponent: name, chips, last action */
#define Y_AICARD  40
#define Y_POT     74
#define Y_BOARD   84
#define Y_ME      122
#define Y_MECARD  132
#define Y_HAND    166
#define Y_MENU    174           /* the action bar: F / C / R fields in three slots */
#define Y_STAT    184
#define SEAT_W4   22            /* an opponent's column is 88 px wide */
#define AI_STEP4  5             /* card pitch of an opponent's hand, in 4-pixel units: 3 (12 px) showed little of each card, 5 shows 8 pixels more; at most (SEAT_W4 - 6) / 3 = 5 so that four cards fit */
#define ME_X4     2
#define ME_STEP4  8             /* card pitch of the player's hand: 32 px, so card k starts at byte column 1 + 4k */
#define BOARD_X4  13
#define BOARD_STEP4 8
#if 3 * AI_STEP4 + 6 > SEAT_W4
#error "AI_STEP4 is too wide: four cards must fit in an opponent column"
#endif
#define MENU_X0   0             /* the action bar's byte columns; the widths (8, 12, 12) belong to the cached fields in play.c */
#define MENU_X1   8
#define MENU_X2   20

/* ---- the commentators (play.c) ---- */
#define QB_X4 14               /* bubble: 192 x 42 pixels, tail 6 more */
#define QB_Y  80
#define QB_W4 48
#define QB_H  40
#define QF_X4 40               /* the two portraits (48 x 48) side by side at the bottom right corner */
#define QF_Y  136

#endif
