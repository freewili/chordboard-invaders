#include "shake.h"

#define SHAKE_G2      6.25f    /* (2.5 g)^2 */
#define SHAKE_LOCK_MS 800u

void shake_init(shake_t *s) { s->lockout_until = 0; }

bool shake_feed(shake_t *s, float ax, float ay, float az, uint32_t now_ms)
{
    float m2 = ax * ax + ay * ay + az * az;
    if (now_ms < s->lockout_until) return false;
    if (m2 < SHAKE_G2) return false;
    s->lockout_until = now_ms + SHAKE_LOCK_MS;
    return true;
}
