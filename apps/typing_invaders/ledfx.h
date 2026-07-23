// ledfx.h - ambient effect layer for the 16-pixel WS2812 strip.
#ifndef LEDFX_H
#define LEDFX_H
#include <stdbool.h>

void ledfx_init(void);
void ledfx_set_level(int level);     /* idle glow color follows the level */
void ledfx_chord_flash(int btn);     /* 0..4 = keycap color segment flash */
void ledfx_danger(bool on);          /* alien near defense line: red pulse */
void ledfx_rainbow(void);            /* level-up sweep */
void ledfx_white(void);              /* smart-bomb burst */
void ledfx_task(void);               /* call every main-loop pass */
#endif
