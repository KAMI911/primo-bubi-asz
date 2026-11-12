#ifndef HALL_H
#define HALL_H
#include <stdint.h>
#include "poker.h"

#define HALL_ENTRIES 3
#define HALL_NAME    8
typedef struct { char name[HALL_NAME + 1]; uint16_t chips; uint16_t hands; } HallRec;

void hall_clear(void);
uint8_t hall_rank(uint8_t game, uint16_t chips);          /* position this result would take, or 0xFF */
void hall_add(uint8_t game, uint8_t rank, const char *name, uint16_t chips, uint16_t hands);
const HallRec *hall_get(uint8_t game, uint8_t i);         /* chips == 0 marks an empty slot */

#endif
