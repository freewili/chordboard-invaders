#include "fb.h"
#include "display/st7796.h"
#include "display/font5x7.h"

static uint16_t *s_fb;

void      fb_set(uint16_t *buf) { s_fb = buf; }
uint16_t *fb_get(void)          { return s_fb; }

uint16_t fb_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t c = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    return (uint16_t)((c >> 8) | (c << 8));
}

void fb_clear(uint16_t color_be)
{
    for (int i = 0; i < ST7796_W * ST7796_H; i++) s_fb[i] = color_be;
}

void fb_fill_rect(int x, int y, int w, int h, uint16_t color_be)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > ST7796_W) w = ST7796_W - x;
    if (y + h > ST7796_H) h = ST7796_H - y;
    for (int yy = y; yy < y + h; yy++) {
        uint16_t *row = s_fb + (long)yy * ST7796_W + x;
        for (int xx = 0; xx < w; xx++) row[xx] = color_be;
    }
}

static void draw_text_impl(int x, int y, int scale, uint16_t fg_be,
                           uint16_t bg_be, int transparent, const char *s)
{
    if (scale < 1) scale = 1;
    if (scale > 4) scale = 4;
    const int w = 6 * scale, h = 8 * scale;
    for (; *s; s++, x += w) {
        if (x + w > ST7796_W || y + h > ST7796_H || x < 0 || y < 0) break;
        char c = *s;
        const uint8_t *cols = (c >= FONT5X7_FIRST && c <= FONT5X7_LAST)
                                  ? font5x7[c - FONT5X7_FIRST]
                                  : font5x7[0];
        for (int gy = 0; gy < h; gy++) {
            int grow = gy / scale;
            uint16_t *row = s_fb + (long)(y + gy) * ST7796_W + x;
            for (int gx = 0; gx < w; gx++) {
                int col = gx / scale;
                int on = col < 5 && grow < 7 && ((cols[col] >> grow) & 1);
                if (on) row[gx] = fg_be;
                else if (!transparent) row[gx] = bg_be;
            }
        }
    }
}

void fb_draw_text(int x, int y, int scale, uint16_t fg_be, uint16_t bg_be,
                  const char *s)
{
    draw_text_impl(x, y, scale, fg_be, bg_be, 0, s);
}

void fb_draw_text_t(int x, int y, int scale, uint16_t fg_be, const char *s)
{
    draw_text_impl(x, y, scale, fg_be, 0, 1, s);
}
