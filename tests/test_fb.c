#include "test_util.h"
#include "fb.h"
#include "display/st7796.h"
#include <stdlib.h>
#include <string.h>

int main(void) {
    uint16_t *buf = calloc(ST7796_W * ST7796_H, 2);
    uint16_t *guard = buf;   /* calloc'd exact size; OOB writes would crash */
    fb_set(buf);
    ASSERT_TRUE(fb_get() == buf);

    uint16_t red = fb_rgb(255, 0, 0);
    fb_clear(red);
    ASSERT_EQ(buf[0], red);
    ASSERT_EQ(buf[ST7796_W * ST7796_H - 1], red);

    /* clipped fill: negative origin and past-edge extent */
    uint16_t blue = fb_rgb(0, 0, 255);
    fb_fill_rect(-10, -10, 20, 20, blue);
    ASSERT_EQ(buf[0], blue);                              /* 10x10 landed */
    ASSERT_EQ(buf[10 * ST7796_W + 10], red);              /* outside kept */
    fb_fill_rect(ST7796_W - 5, ST7796_H - 5, 50, 50, blue);
    ASSERT_EQ(buf[ST7796_H * ST7796_W - 1], blue);

    /* text draws pixels; transparent variant leaves bg alone */
    fb_clear(0);
    uint16_t white = fb_rgb(255, 255, 255);
    fb_draw_text(0, 0, 2, white, red, "A");
    int fg = 0, bg = 0;
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 12; x++) {
            if (buf[y * ST7796_W + x] == white) fg++;
            if (buf[y * ST7796_W + x] == red) bg++;
        }
    ASSERT_TRUE(fg > 10); ASSERT_TRUE(bg > 10);
    fb_clear(red);
    fb_draw_text_t(0, 0, 2, white, "A");
    int untouched = 0;
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 12; x++)
            if (buf[y * ST7796_W + x] == red) untouched++;
    ASSERT_TRUE(untouched > 10);                          /* bg preserved */

    /* out-of-bounds text: no crash, buffer intact */
    fb_draw_text(ST7796_W - 3, ST7796_H - 3, 4, white, red, "XYZ");
    (void)guard;
    free(buf);
    TEST_RETURN();
}
