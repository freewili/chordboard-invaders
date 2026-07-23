// typing_invaders - main entry. Task 1 skeleton: bring the board up, prove
// display + PSRAM + RTT, and show a placeholder title screen.
#include "fw2.h"
#include "platform/diag.h"
#include "platform/psram.h"
#include "pico/stdlib.h"

static inline uint16_t rgb565_be(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t c = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    return (uint16_t)((c >> 8) | (c << 8));
}

int main(void) {
    board_init();
    size_t psram_bytes = psram_init();
    st7796_init();
    board_backlight_set(1);
    st7796_fill_screen(rgb565_be(0, 0, 24));
    st7796_draw_text(60, 120, 4, rgb565_be(120, 255, 120), rgb565_be(0, 0, 24),
                     "TYPING");
    st7796_draw_text(60, 160, 4, rgb565_be(255, 80, 80), rgb565_be(0, 0, 24),
                     "INVADERS");
    DIAG("typing_invaders skeleton up, psram=%u bytes\n", (unsigned)psram_bytes);
    for (;;) {
        sleep_ms(1000);
        DIAG("tick\n");
    }
}
