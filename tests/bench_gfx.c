/* Cycle count of the drawing routines on the emulated Z80 (z88dk-ticks): one workload per run, -DWORK=n, the total tick count of
   a run minus the one of WORK=0 is the cost of the workload. The hardware layer is a plain RAM screen. */
#include <stdint.h>
#include <string.h>
#include "../src/gfx.h"
#include "../src/hw.h"
#include "../src/gfx.c"

static uint8_t vram_raw[6144 + 32], cramb[50 * 32];
#define vram ((uint8_t *)(((uint16_t)vram_raw + 31) & ~31u))
uint8_t *hw_vram(void) { return vram; }
uint8_t *hw_color_origin(void) { return COLOR ? cramb : 0; }
uint8_t *hw_color_ram(void) { return COLOR ? cramb : 0; }
uint8_t hw_is_color(void) { return COLOR; }
void hw_color_setup(const uint16_t *pal16, uint8_t table) { (void)pal16; (void)table; }
void hw_poll(void) { }
uint16_t hw_frames(void) { return 0; }
void hw_wait_frame(void) { }
const uint8_t pk_tok[1][2] = {{0, 0}};

int main(void) {
    uint16_t i;
    gfx_init();
#if WORK == 1
    for (i = 0; i < 20; i++) gfx_clear();                                                  /* 20 full clears */
#elif WORK == 2
    for (i = 0; i < 200; i++) gfx_text(0, (uint8_t)((i % 20) * 9), "THE QUICK BROWN FOX JUMPS OVER ONE", (uint8_t)(i & 1));   /* 200 lines of 32 characters */
#elif WORK == 3
    for (i = 0; i < 100; i++) gfx_fill(14, (uint8_t)(i % 60), 48, 46, (uint8_t)(i & 1));   /* 100 bubble-sized fills */
#elif WORK == 4
    for (i = 0; i < 200; i++) gfx_card((uint8_t)((i % 8) * 8), (uint8_t)((i % 5) * 34), (card_t)((i % 52) | C_UP), 0);   /* 200 cards */
#endif
    (void)i;
    return 0;
}
