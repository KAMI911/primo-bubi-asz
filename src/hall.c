#include "hall.h"

static HallRec table[NPGAMES][HALL_ENTRIES];

void hall_clear(void) {
    uint8_t g, i, k;
    for (g = 0; g < NPGAMES; g++)
        for (i = 0; i < HALL_ENTRIES; i++) {
            for (k = 0; k <= HALL_NAME; k++) table[g][i].name[k] = 0;
            table[g][i].chips = 0; table[g][i].hands = 0;
        }
}

uint8_t hall_rank(uint8_t game, uint16_t chips) {
    uint8_t i;
    if (game >= NPGAMES || chips == 0) return 0xFF;
    for (i = 0; i < HALL_ENTRIES; i++) if (chips > table[game][i].chips) return i;      /* a tie keeps the older entry first */
    return 0xFF;
}

void hall_add(uint8_t game, uint8_t rank, const char *name, uint16_t chips, uint16_t hands) {
    uint8_t i, k;
    if (game >= NPGAMES || rank >= HALL_ENTRIES) return;
    for (i = (uint8_t)(HALL_ENTRIES - 1); i > rank; i--) table[game][i] = table[game][i - 1];
    for (k = 0; k < HALL_NAME && name[k]; k++) table[game][rank].name[k] = name[k];
    for (; k <= HALL_NAME; k++) table[game][rank].name[k] = 0;
    table[game][rank].chips = chips;
    table[game][rank].hands = hands;
}

const HallRec *hall_get(uint8_t game, uint8_t i) {
    static const HallRec empty = {{0}, 0, 0};
    return (game < NPGAMES && i < HALL_ENTRIES) ? &table[game][i] : &empty;
}
