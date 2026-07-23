// shake.h - debounced shake detector over accel samples (units: g).
// Fires once when |a| > 2.5 g, then locks out for 800 ms.
#ifndef SHAKE_H
#define SHAKE_H
#include <stdbool.h>
#include <stdint.h>

typedef struct { uint32_t lockout_until; } shake_t;

void shake_init(shake_t *s);
bool shake_feed(shake_t *s, float ax, float ay, float az, uint32_t now_ms);

#endif
