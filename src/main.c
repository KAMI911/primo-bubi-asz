/* Own stack. Monochrome model: just below the screen ($E800). Colour model: $E000-$E7FF is control, palette and
   colour RAM (pixels start at $E700), so user RAM ends at $DFFF and the stack goes below that. */
#pragma output CLIB_FOPEN_MAX = 0      /* no stdio: the 100 byte file table is not needed */
#ifdef TARGET_COLOR
#pragma output REGISTER_SP = 0xDFF0
#else
#pragma output REGISTER_SP = 0xE7F0
#endif

#include "gfx.h"
#include "hall.h"
#include "hw.h"
#include "input.h"
#include "play.h"
#include "ui.h"

void main(void) {
    uint8_t choice, last = 0;
    hw_init();
    gfx_init();
    hall_clear();
    gfx_set_invert(cfg.invert);
    gfx_set_table(cfg.table);
    gfx_set_back_colour(cfg.bcol);
    ui_title();
    ui_name();
    for (;;) {
        choice = ui_menu(play_active(), &last);
        if (choice < NPGAMES) play_session(choice, 0);
        else if (choice >= UI_RULES_BASE && choice < UI_RULES_BASE + NPGAMES) ui_rules((uint8_t)(choice - UI_RULES_BASE));
        else switch (choice) {
            case UI_CONTINUE: if (play_active()) play_session(play_game_index(), 1); break;
            case UI_HALL: ui_hall(last); break;
            case UI_OPTIONS: ui_options(); break;
            case UI_HELP: ui_help(); break;
            case UI_NAME: ui_name(); break;
            case UI_QUIT: gfx_clear(); return;           /* back to BASIC */
            default: break;
        }
    }
}
