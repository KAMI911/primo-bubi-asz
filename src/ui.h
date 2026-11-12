#ifndef UI_H
#define UI_H
#include <stdint.h>

typedef struct {
    uint8_t back;        /* card back design 0..NBACKS-1 */
    uint8_t scheme;      /* control scheme (input.h) */
    uint8_t sound;
    uint8_t invert;      /* swap black/white if a model displays the opposite polarity */
    uint8_t table;       /* colour model: table colour 0 green, 1 burgundy, 2 black */
    uint8_t bcol;        /* colour model: card back colour 0 blue, 1 red, 2 green */
    uint8_t chat;        /* 0 off, 1 Gyorgy and Klarika comment on the game, 2 (DRUNK) they talk a lot */
    char    name[9];
} Settings;
extern Settings cfg;

enum { UI_CONTINUE = 100, UI_HALL, UI_OPTIONS, UI_HELP, UI_NAME, UI_QUIT, UI_RULES_BASE = 120 };

void    ui_snd(uint8_t kind);                 /* 0 select, 1 error, 2 good, 3 win */
void    ui_title(void);
void    ui_name(void);                        /* edit cfg.name */
uint8_t ui_menu(uint8_t can_continue, uint8_t *last_game);   /* returns a game index or a UI_ code */
void    ui_options(void);
void    ui_hall(uint8_t game);
void    ui_rules(uint8_t game);
void    ui_help(void);
void    ui_wait_key(void);
const char *ui_game_name(uint8_t game);
const char *ui_hand_name(uint8_t cat);        /* "FLUSH" ... */

#endif
