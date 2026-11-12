/* Host stand-in for the hardware: a RAM screen, a frame counter and scripted key presses. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/hw.h"
#include "hw_host.h"
void host_dump(const char *path);

static uint8_t screen[200 * 32];
static uint8_t cbuf[0x1000];                  /* host image of $E000..$EFFF: control, palettes, colour RAM at +0x80 */
static int color_on;
void host_color(int on) { color_on = on; }
static unsigned shot_frame[8]; static const char *shot_path[8]; static unsigned nshots;
static uint16_t frame_ctr;
static uint8_t keys[64];
static const HostEv *script;
static unsigned script_n, script_pos;

void host_script(const HostEv *ev, unsigned n) {
    script = ev; script_n = n; script_pos = 0; frame_ctr = 0;
    { unsigned i; for (i = 0; i < 64; i++) keys[i] = 0; }
}
unsigned host_script_left(void) { return script_n - script_pos; }

static void apply(void) {
    while (script_pos < script_n && script[script_pos].frame <= frame_ctr) {
        keys[script[script_pos].sc & 63] = script[script_pos].down;
        script_pos++;
    }
}

void     hw_init(void) { }
uint8_t  hw_is_color(void) { return (uint8_t)color_on; }
uint8_t *hw_color_ram(void) { return color_on ? cbuf + 0x80 : 0; }
uint8_t *hw_color_origin(void) { return hw_color_ram(); }
uint8_t *hw_vram(void) { return screen; }
uint8_t  hw_key(uint8_t sc) { return keys[sc & 63]; }
void     hw_beep(uint8_t pitch, uint8_t length) { (void)pitch; (void)length; }
void     hw_poll(void) { }
uint16_t hw_frames(void) { return frame_ctr; }
void     hw_wait_frame(void) {
    frame_ctr++;
    apply();
    { unsigned k; for (k = 0; k < nshots; k++) if (shot_frame[k] == frame_ctr && shot_frame[k]) { if (color_on) host_dump_color(shot_path[k]); else host_dump(shot_path[k]); shot_frame[k] = 0xFFFFFFFFu; } }
    if (frame_ctr > 60000u) { fprintf(stderr, "host: script ran out (frame limit)\n"); exit(3); }
}

void host_shot(unsigned frame, const char *path) {       /* slots are reused once their shot has been taken */
    unsigned k;
    for (k = 0; k < 8; k++) if (shot_frame[k] == 0 || shot_frame[k] == 0xFFFFFFFFu) { shot_frame[k] = frame; shot_path[k] = path; if (k >= nshots) nshots = k + 1; return; }
}

/* text dump in the same format the Z80 emulator run prints: pixels, colour RAM, first palette bank */
void host_dump_color(const char *path) {
    FILE *f = fopen(path, "w");
    unsigned i;
    if (!f) return;
    fprintf(f, "SHOTC 00\n");
    for (i = 0; i < 192 * 32; i++) { fprintf(f, "%02X", screen[i]); if ((i & 31) == 31) fputc('\n', f); }
    for (i = 0; i < 50 * 32; i++) { fprintf(f, "%02X", cbuf[0x80 + i]); if ((i & 31) == 31) fputc('\n', f); }
    for (i = 0; i < 32; i++) fprintf(f, "%02X", cbuf[0x20 + i]);
    fprintf(f, "\nEND\n");
    fclose(f);
}

void host_dump(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P4\n256 192\n");
    fwrite(screen, 1, 32 * 192, f);
    fclose(f);
}
