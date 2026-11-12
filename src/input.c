#include "input.h"
#include "hw.h"
#include "ptext.h"

/* Scan codes (0..63) from the z88dk Primo key table (libsrc/target/primo/input/in_keytranstbl.asm),
   cross-checked with the schematic in the Hardver book (Fig. 5.4.3) and the Raktaros sources. */
#define SC_UP     0x01
#define SC_DOWN   0x0F
#define SC_LEFT   0x39
#define SC_RIGHT  0x3D
#define SC_SPACE  0x19
#define SC_RETURN 0x37
#define SC_BREAK  0x3F
#define SC_CLS    0x35        /* the key above RETURN: used as backspace */

/* index = scan code, value = character or 0 */
static const char sc2ch[64] = {
    'Y', 0,  'S', 0,  'E', 0,  'W', 0,    'D', '3', 'X', '2', 'Q', '1', 'A', 0,
    'C', 0,  'F', 0,  'R', 0,  'T', '7',  'H', ' ', 'B', '6', 'G', '5', 'V', '4',
    'N', '8', 'Z', 0, 'U', '0', 'J', 0,   'L', 0,  'K', 0,  'M', '9', 'I', 0,
    0,   0,  'P', 0,  'O', 0,  0,  0,     0,   0,  0,   0,  0,   0,  0,   0
};

char input_scancode_char(uint8_t sc) { return sc < 64 ? sc2ch[sc] : 0; }

static uint8_t scheme, text_mode;
void input_set_text(uint8_t on) { text_mode = on; }
void input_set_scheme(uint8_t s) { scheme = (uint8_t)(s % NSCHEMES); }
uint8_t input_scheme(void) { return scheme; }
const char *input_scheme_name(uint8_t s) {
    static const char *const n[NSCHEMES] = {T("CURSOR KEYS", "NYILAK"), T("NUMBERS 8246", "SZÁMOK 8246"), "Q A O P"};
    return n[s % NSCHEMES];
}

/* extra direction keys per scheme: up, down, left, right, fire */
static const uint8_t scheme_keys[NSCHEMES][5] = {
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    {0x21, 0x0B, 0x1F, 0x1B, 0x1D},      /* 8 2 4 6, fire 5 */
    {0x0C, 0x0E, 0x34, 0x32, 0xFF},      /* Q A O P */
};

#define PRESSED(sc) ((sc) != 0xFF && hw_key(sc))

static uint8_t dir_down(uint8_t d) {          /* d: 0 up, 1 down, 2 left, 3 right, 4 select */
    static const uint8_t base[5] = {SC_UP, SC_DOWN, SC_LEFT, SC_RIGHT, SC_SPACE};
    if (PRESSED(base[d])) return 1;
    if (!text_mode) {                                  /* always on: I and U up, K down, J left, L right (letters in name entry) */
        static const uint8_t alt[4][2] = {{0x2E, 0x24}, {0x2A, 0xFF}, {0x26, 0xFF}, {0x28, 0xFF}};
        if (d < 4 && (PRESSED(alt[d][0]) || PRESSED(alt[d][1]))) return 1;
    }
    if (d == 4 && PRESSED(SC_RETURN)) return 1;
    return (uint8_t)(!text_mode && PRESSED(scheme_keys[scheme][d]));
}

/* letter keys that are actions: scan code -> event */
static const struct { uint8_t sc; uint8_t ev; } act[] = {
    {0x16, KEY_HINT},      /* T */
    {0x12, KEY_FOLD},      /* F */
    {0x10, KEY_CALL},      /* C */
    {0x14, KEY_RAISE},     /* R */
    {0x2C, KEY_MENU},      /* M */
    {0x22, KEY_PAUSE},     /* Z */
    {0x02, KEY_SOUND},     /* S */
    {SC_BREAK, KEY_BACK},
    {SC_CLS, KEY_BKSP},
};
#define NACT ((uint8_t)(sizeof(act) / sizeof(act[0])))

static uint8_t held[8];                       /* previous state: bit i = key i of the list below */
static uint8_t rep_wait, rep_dir = 0xFF;
static char last_char;

static uint8_t bit_get(uint8_t i) { return (uint8_t)((held[i >> 3] >> (i & 7)) & 1); }
static void bit_set(uint8_t i, uint8_t v) {
    if (v) held[i >> 3] |= (uint8_t)(1 << (i & 7)); else held[i >> 3] &= (uint8_t)~(1 << (i & 7));
}

/* indexes: 0..4 directions/select, 5.. actions, 32.. characters 0..63 map to bit 64+sc -> use second array */
static uint8_t chheld[8];

uint8_t input_poll(void) {
    uint8_t d, i, now, ev = KEY_NONE, sc;
    static const uint8_t direv[5] = {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_SELECT};
    for (d = 0; d < 5; d++) {
        now = dir_down(d);
        if (now && !bit_get(d)) { bit_set(d, 1); rep_dir = d; rep_wait = 10; if (ev == KEY_NONE) ev = direv[d]; }
        else if (now && d == rep_dir && d < 4) {                 /* auto-repeat for directions */
            if (rep_wait) rep_wait--;
            else { rep_wait = 2; if (ev == KEY_NONE) ev = direv[d]; }
        }
        else if (!now) { if (bit_get(d) && rep_dir == d) rep_dir = 0xFF; bit_set(d, 0); }
    }
    for (i = 0; i < NACT; i++) {
        if (text_mode && act[i].ev != KEY_BACK && act[i].ev != KEY_BKSP) continue;
        now = hw_key(act[i].sc);
        if (now && !bit_get((uint8_t)(8 + i))) { bit_set((uint8_t)(8 + i), 1); if (ev == KEY_NONE) ev = act[i].ev; }
        else if (!now) bit_set((uint8_t)(8 + i), 0);
    }
    for (sc = 0; sc < 64; sc++) {                                 /* typed characters (name entry) */
        if (!sc2ch[sc]) continue;
        now = hw_key(sc);
        if (now && !((chheld[sc >> 3] >> (sc & 7)) & 1)) {
            chheld[sc >> 3] |= (uint8_t)(1 << (sc & 7));
            if (ev == KEY_NONE) { ev = KEY_CHAR; last_char = sc2ch[sc]; }
        } else if (!now) chheld[sc >> 3] &= (uint8_t)~(1 << (sc & 7));
    }
    return ev;
}

char input_char(void) { return last_char; }

uint8_t input_any_down(void) {
    uint8_t sc;
    for (sc = 0; sc < 64; sc++) if (sc != 3 && sc != 5 && sc != 7 && hw_key(sc)) return 1;   /* ignore SHIFT/UPPER/CTR */
    return 0;
}

void input_wait_release(void) {
    uint8_t n = 0;
    while (n < 3) { hw_wait_frame(); if (input_any_down()) n = 0; else n++; }
    for (n = 0; n < 8; n++) held[n] = chheld[n] = 0;
    rep_dir = 0xFF;
}
