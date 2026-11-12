#ifndef PLAY_H
#define PLAY_H
#include <stdint.h>

/* Plays a session at the table (resume = 1 continues the one left through the menu). */
void    play_session(uint8_t game, uint8_t resume);
uint8_t play_active(void);
uint8_t play_game_index(void);
/* the rules page lines (packed with the comments): group 0..1 = a game (PG_ order), 2 = the part common to all games; line k, or 0 after the last one.
   The text is valid until the next call. */
const char *play_rules_line(uint8_t group, uint8_t k);
/* host tests: shows comment number n over the screen and takes it away again */
void    play_quip_test(uint8_t n);
#endif
