#ifndef INPUT_H
#define INPUT_H
#include <stdint.h>

enum {
    KEY_NONE = 0, KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN, KEY_SELECT, KEY_BACK,
    KEY_HINT, KEY_MENU, KEY_PAUSE, KEY_SOUND,
    KEY_FOLD, KEY_CALL, KEY_RAISE,
    KEY_CHAR,                                    /* a typed character: see input_char() */
    KEY_BKSP
};
#define NSCHEMES 3                               /* 0 cursor keys, 1 numeric 8/2/4/6, 2 QAOP; I/U up, K down, J left, L right always work too */

void    input_set_text(uint8_t on);              /* 1: letters are typed characters only (name entry) */
void    input_set_scheme(uint8_t s);
uint8_t input_scheme(void);
const char *input_scheme_name(uint8_t s);
/* One event per call, or KEY_NONE. Direction keys repeat while held. Call once per frame. */
uint8_t input_poll(void);
char    input_char(void);                        /* the character of the last KEY_CHAR event (A-Z, 0-9, space) */
uint8_t input_any_down(void);                    /* 1 if any key we know is held (used by "press any key") */
void    input_wait_release(void);

/* translation helpers, exported for tests */
char    input_scancode_char(uint8_t sc);         /* 'A'..'Z','0'..'9',' ' or 0 */

#endif
