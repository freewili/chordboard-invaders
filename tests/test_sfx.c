#include "test_util.h"
#include "sfx.h"
#include <string.h>

static int nonzero(const int16_t *b, int n) {
    for (int i = 0; i < n; i++) if (b[i]) return 1;
    return 0;
}

int main(void) {
    sfx_t s;
    int16_t buf[2048];

    /* silence when idle */
    sfx_init(&s);
    sfx_render(&s, buf, 2048);
    ASSERT_TRUE(!nonzero(buf, 2048));

    /* zap: sound now, silence after its ~60 ms run out */
    sfx_trigger(&s, SFX_ZAP, 1);
    sfx_render(&s, buf, 1024);            /* 64 ms */
    ASSERT_TRUE(nonzero(buf, 1024));
    sfx_render(&s, buf, 1024);
    ASSERT_TRUE(!nonzero(buf, 1024));

    /* deterministic: same trigger renders identical samples */
    int16_t buf2[512];
    sfx_init(&s); sfx_trigger(&s, SFX_ZAP, 4);
    sfx_render(&s, buf, 512);
    sfx_init(&s); sfx_trigger(&s, SFX_ZAP, 4);
    sfx_render(&s, buf2, 512);
    ASSERT_TRUE(memcmp(buf, buf2, sizeof buf2) == 0);

    /* all voices at once stay in int16 range (clamped, no wrap) */
    sfx_init(&s);
    sfx_trigger(&s, SFX_ZAP, 8);
    sfx_trigger(&s, SFX_BOSS_BOOM, 0);
    sfx_trigger(&s, SFX_LEVELUP, 0);
    sfx_render(&s, buf, 2048);
    ASSERT_TRUE(nonzero(buf, 2048));      /* something audible */

    /* level-up jingle spans several steps: still audible at 300 ms */
    sfx_init(&s); sfx_trigger(&s, SFX_LEVELUP, 0);
    sfx_render(&s, buf, 2048);            /* skip 128 ms */
    sfx_render(&s, buf, 2048);            /* 128..256 ms */
    sfx_render(&s, buf, 1024);            /* 256..320 ms */
    ASSERT_TRUE(nonzero(buf, 1024));

    /* title loop keeps playing far beyond one pass; stop silences it */
    sfx_init(&s); sfx_trigger(&s, SFX_TITLE_LOOP, 0);
    for (int i = 0; i < 30; i++) sfx_render(&s, buf, 2048);   /* ~3.8 s */
    ASSERT_TRUE(nonzero(buf, 2048));
    sfx_trigger(&s, SFX_LOOP_STOP, 0);
    sfx_render(&s, buf, 2048);
    ASSERT_TRUE(!nonzero(buf, 2048));

    /* noise voice (kill) is audible and decays out */
    sfx_init(&s); sfx_trigger(&s, SFX_KILL, 0);
    sfx_render(&s, buf, 2048);
    ASSERT_TRUE(nonzero(buf, 2048));
    sfx_render(&s, buf, 2048);
    sfx_render(&s, buf, 2048);
    ASSERT_TRUE(!nonzero(buf, 2048));
    TEST_RETURN();
}
