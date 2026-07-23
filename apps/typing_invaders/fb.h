// fb.h - software framebuffer helpers over a caller-owned 480x320 wire-order
// RGB565 buffer (PSRAM on target, malloc in host tests). Pure: no hardware.
#ifndef FB_H
#define FB_H
#include <stdint.h>

void      fb_set(uint16_t *buf);
uint16_t *fb_get(void);
void      fb_clear(uint16_t color_be);
void      fb_fill_rect(int x, int y, int w, int h, uint16_t color_be);
void      fb_draw_text(int x, int y, int scale, uint16_t fg_be, uint16_t bg_be,
                       const char *s);
void      fb_draw_text_t(int x, int y, int scale, uint16_t fg_be, const char *s);
uint16_t  fb_rgb(uint8_t r, uint8_t g, uint8_t b);   /* big-endian RGB565 */

#endif
