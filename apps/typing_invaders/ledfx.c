#include "ledfx.h"
#include "fw2.h"
#include "pico/stdlib.h"

static const rgb_t k_btn[5] = {
    { 120, 118, 120 }, { 255, 227, 49 }, { 40, 160, 20 },
    { 20, 60, 255 }, { 220, 20, 70 } };
static const rgb_t k_level[8] = {
    { 0, 40, 0 }, { 0, 30, 30 }, { 0, 0, 50 }, { 40, 30, 0 },
    { 40, 0, 40 }, { 50, 20, 0 }, { 30, 30, 30 }, { 50, 0, 0 } };

static rgb_t    s_base;
static bool     s_danger;
static int      s_flash_btn = -1;
static uint64_t s_flash_until, s_rainbow_until, s_white_until, s_next_show;

void ledfx_init(void)
{
    ws2812_init(pio1, 0, 21);        /* PIN_LED_DATA (pinmap.md) */
    ws2812_set_brightness(20);
    s_base = k_level[0];
}

void ledfx_set_level(int level) { s_base = k_level[(level - 1) & 7]; }
void ledfx_chord_flash(int btn)
{
    if (btn < 0 || btn > 4) return;
    s_flash_btn = btn;
    s_flash_until = time_us_64() + 150000;
}
void ledfx_danger(bool on) { s_danger = on; }
void ledfx_rainbow(void) { s_rainbow_until = time_us_64() + 600000; }
void ledfx_white(void)   { s_white_until = time_us_64() + 300000; }

static rgb_t wheel(unsigned p)       /* 0..255 -> color wheel */
{
    p &= 255;
    if (p < 85)  return (rgb_t){ (uint8_t)(255 - p * 3), (uint8_t)(p * 3), 0 };
    if (p < 170) { p -= 85; return (rgb_t){ 0, (uint8_t)(255 - p * 3), (uint8_t)(p * 3) }; }
    p -= 170;    return (rgb_t){ (uint8_t)(p * 3), 0, (uint8_t)(255 - p * 3) };
}

void ledfx_task(void)
{
    uint64_t now = time_us_64();
    if (now < s_next_show) return;   /* ~60 Hz cap */
    s_next_show = now + 16000;

    for (unsigned i = 0; i < FW2_LED_COUNT; i++) {
        rgb_t c = s_base;
        if (s_danger) {
            unsigned tri = (unsigned)(now / 4000) & 255;      /* ~1 Hz pulse */
            if (tri > 127) tri = 255 - tri;
            c = (rgb_t){ (uint8_t)(60 + tri), 0, 0 };
        }
        if (now < s_flash_until && s_flash_btn >= 0) {
            unsigned seg = i * 5u / FW2_LED_COUNT;            /* 5 segments */
            if ((int)seg == s_flash_btn) c = k_btn[s_flash_btn];
        }
        if (now < s_rainbow_until)
            c = wheel((unsigned)(i * 16 + now / 3000));
        if (now < s_white_until)
            c = (rgb_t){ 255, 255, 255 };
        ws2812_set_pixel(i, c);
    }
    ws2812_show();
}
