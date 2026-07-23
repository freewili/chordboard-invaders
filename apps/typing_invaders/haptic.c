#include "haptic.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

#define PIN_HAPTIC 46

static uint64_t s_off_at;          /* current pulse end */
static uint64_t s_next_on_at;      /* queued second pulse start */
static uint32_t s_next_on_ms;

void haptic_init(void)
{
    gpio_init(PIN_HAPTIC);
    gpio_set_dir(PIN_HAPTIC, GPIO_OUT);
    gpio_put(PIN_HAPTIC, 0);
}

void haptic_pulse(uint32_t ms)
{
    gpio_put(PIN_HAPTIC, 1);
    s_off_at = time_us_64() + (uint64_t)ms * 1000;
    s_next_on_at = 0;
}

void haptic_double(uint32_t on_ms, uint32_t gap_ms)
{
    haptic_pulse(on_ms);
    s_next_on_at = s_off_at + (uint64_t)gap_ms * 1000;
    s_next_on_ms = on_ms;
}

void haptic_task(void)
{
    uint64_t now = time_us_64();
    if (s_off_at && now >= s_off_at) {
        gpio_put(PIN_HAPTIC, 0);
        s_off_at = 0;
    }
    if (s_next_on_at && now >= s_next_on_at) {
        uint32_t ms = s_next_on_ms;
        s_next_on_at = 0;
        haptic_pulse(ms);
    }
}
