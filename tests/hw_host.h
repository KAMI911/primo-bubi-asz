#ifndef HW_HOST_H
#define HW_HOST_H
typedef struct { unsigned frame; unsigned char sc; unsigned char down; } HostEv;
void host_script(const HostEv *ev, unsigned n);   /* key events applied as frames pass */
unsigned host_script_left(void);
void host_dump(const char *path);
void host_color(int on);                        /* pretend to be the colour model (call before gfx_init) */
void host_dump_color(const char *path);
void host_shot(unsigned frame, const char *path);   /* save a screenshot when that frame is reached */
#endif
