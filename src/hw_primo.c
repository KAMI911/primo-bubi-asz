/* The real Primo. Verified facts: see references/hardware.md of the primo-developer-tester skill. */
#include <arch/z80.h>
#include "hw.h"

#define PORT0        0x00
#define IN_VSYNC     0x20            /* input bit D5: 1 during frame blanking */
#define SHADOW_PORT0 ((volatile uint8_t *)0x403B)
#define SOUND_BIT    0x10            /* output bit D4 */

/* The colour model's picture is 256 x 200; the game uses 192 lines. They start COLOR_TOP_LINES lines down (a multiple of
   4: whole colour cells) so the picture is centred instead of sitting at the top. Reported as "shifted up" on a real
   screen, so this is a guess to tune: 8 = at the bottom (nothing hidden at the top), 4 = centred, 0 = top. */
#define COLOR_TOP_LINES 8
static uint8_t  color;
static uint8_t *vram;
static uint8_t  last_vs;
static volatile uint16_t frames;

/* The ROM keeps the first address of the pixel memory in the word at $0013 (z88dk's Primo startup reads it
   the same way): $E800 on a 64K monochrome machine, lower on smaller ones. Anything implausible falls back to $E800. */
#define SCREEN_BASE_WORD ((const volatile uint16_t *)0x0013)

void hw_init(void) {
    uint16_t base = *SCREEN_BASE_WORD;
#if defined(TARGET_BW)
    color = 0;
#elif defined(TARGET_COLOR)
    color = 1;
#else
    color = (*(const volatile uint8_t *)0 == 0xC3);   /* the colour ROM starts with JP; same test as Raktaros */
#endif
    if (base < 0x6800 || base > 0xE800) base = 0xE800;
    vram = (uint8_t *)(color ? 0xE700 + COLOR_TOP_LINES * 32 : base);
    if (color) {                                      /* wipe control, palette, colour RAM and pixels first (as Raktaros): a reset */
        uint8_t *q;                                   /* leaves whatever the ROM or the last program put in the control area; */
        uint16_t n;                                   /* gfx_init then programs the palette and $E001 */
        for (q = (uint8_t *)0xE000, n = 0; n < 0x2000; n++) q[n] = 0;
    }
    last_vs = (uint8_t)(z80_inp(PORT0) & IN_VSYNC);
    frames = 0;
}

uint8_t  hw_is_color(void) { return color; }
uint8_t *hw_vram(void) { return vram; }
uint8_t *hw_color_ram(void) { return color ? (uint8_t *)0xE080 : 0; }           /* control/palette addresses are relative to this */
uint8_t *hw_color_origin(void) { return color ? (uint8_t *)(0xE080 + 32 * (COLOR_TOP_LINES / 4)) : 0; }
uint8_t  hw_key(uint8_t sc) { return (uint8_t)(z80_inp((uint16_t)(sc & 63)) & 1); }

void hw_poll(void) {
    uint8_t v = (uint8_t)(z80_inp(PORT0) & IN_VSYNC);
    if (v && !last_vs) frames++;
    last_vs = v;
}
uint16_t hw_frames(void) { hw_poll(); return frames; }

/* Waits for the next frame start. The guard keeps the program alive on a machine or emulator whose blanking bit
   never toggles: after ~25 ms of polling the frame counter is advanced by hand instead of waiting forever. */
void hw_wait_frame(void) {
    uint16_t f = hw_frames();
    uint16_t guard = 500;
    while (hw_frames() == f) {
        if (!--guard) { frames++; break; }
    }
}

/* Port 0 is write-only: the mirror at $403B holds the value the ROM keeps writing. We toggle only the
   sound bit and always finish by writing the mirror value back, so NMI/screen/tape bits are never lost. */
void hw_beep(uint8_t pitch, uint8_t length) {
    uint8_t base = (uint8_t)(*SHADOW_PORT0 & (uint8_t)~SOUND_BIT);
    volatile uint8_t n;                    /* volatile: the optimiser must not drop the delay loops */
    uint8_t d;
    uint16_t cycles = (uint16_t)length * 4u;
    while (cycles--) {
        z80_outp(PORT0, (uint8_t)(base | SOUND_BIT));
        for (d = pitch, n = 0; n < d; n++) { }
        z80_outp(PORT0, base);
        for (d = pitch, n = 0; n < d; n++) { }
    }
    z80_outp(PORT0, *SHADOW_PORT0);
}
