#include "test_util.h"
#include "aliens.h"
#include <string.h>

int main(void) {
    /* art integrity: both frames, every row exactly `cols` chars, `rows` rows */
    for (int sp = 0; sp < SP_COUNT; sp++) {
        const alien_def_t *d = alien_def((species_t)sp);
        ASSERT_TRUE(d != 0);
        ASSERT_TRUE(d->rows >= 2 && d->rows <= 4);
        ASSERT_TRUE(d->cols >= 3 && d->cols <= 11);
        for (int f = 0; f < 2; f++)
            for (int r = 0; r < d->rows; r++) {
                ASSERT_TRUE(d->art[f][r] != 0);
                ASSERT_EQ((int)strlen(d->art[f][r]), d->cols);
            }
        ASSERT_TRUE(d->speed_pct > 0 && d->score_pct > 0);
    }
    /* level gating: level 1 only drifters; boss never chosen */
    for (uint32_t r = 0; r < 500; r++) {
        ASSERT_EQ(alien_choose(1, r * 2654435761u), SP_DRIFTER);
        species_t s = alien_choose(9, r * 2654435761u);
        ASSERT_TRUE(s != SP_BOSS);
    }
    ASSERT_EQ(alien_choose(0, 123u), SP_DRIFTER);   /* defensive: no eligible species */
    /* by level 5 all four regular species appear */
    int seen[SP_COUNT] = { 0 };
    for (uint32_t r = 0; r < 2000; r++) seen[alien_choose(5, r * 2654435761u)]++;
    ASSERT_TRUE(seen[SP_DRIFTER] > 0);
    ASSERT_TRUE(seen[SP_ZIGZAG] > 0);
    ASSERT_TRUE(seen[SP_DIVER] > 0);
    ASSERT_TRUE(seen[SP_SHIELDED] > 0);
    TEST_RETURN();
}
