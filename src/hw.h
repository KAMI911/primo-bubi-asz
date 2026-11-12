#ifndef HW_H
#define HW_H
#include <stdint.h>

/* Hardware access. Two implementations: hw_primo.c (the real machine) and tests/hw_host.c (a RAM screen). */
void     hw_init(void);                 /* detect the model, set up the screen */
uint8_t  hw_is_color(void);
uint8_t *hw_color_origin(void);          /* colour cell of the game area's top-left corner (hw_color_ram() plus the unused top lines), 0 on a monochrome model */
uint8_t *hw_color_ram(void);            /* colour attribute RAM ($E080 on the colour Primo), 0 on a monochrome model */
void     hw_color_setup(const uint16_t *pal16, uint8_t table);   /* load the 16-word palette (colour 0, the table, replaced by `table`, RRRGGGBB) into the three palette banks, select the first (hw_color.c) */
uint8_t *hw_vram(void);                 /* start of the pixel area: 32 bytes per line, 1 bit per pixel */
uint8_t  hw_key(uint8_t scancode);      /* 1 while the key is held down (scan codes 0..63) */
void     hw_beep(uint8_t pitch, uint8_t length);   /* pitch: larger = lower; length in 1/100 s approx. */

/* Frame clock: counts 50 Hz frame starts. hw_poll() must run at least once per ~3 ms
   (it is called from the drawing loops) so no frame is ever missed. */
void     hw_poll(void);
uint16_t hw_frames(void);               /* frames since start, wraps at 65536 (21 minutes) */
void     hw_wait_frame(void);           /* returns right after the next frame starts */

#endif
