#include "test_util.h"
#include "render.h"
#include "game.h"
#include "words.h"
#include "hiscore.h"
#include "fb.h"
#include "display/st7796.h"
#include <stdlib.h>

static int nonzero_px(const uint16_t *b) {
    int n = 0;
    for (int i = 0; i < ST7796_W * ST7796_H; i++) n += (b[i] != 0);
    return n;
}

int main(void) {
    uint16_t *buf = calloc(ST7796_W * ST7796_H, 2);
    fb_set(buf);

    game_t g;
    words_init(3); game_init(&g, 3); game_start(&g);
    game_force_spawn(&g, SP_DRIFTER, "cat", 50);
    game_force_spawn(&g, SP_ZIGZAG, "dog", 200)->y_q8 = 100 << 8;
    game_force_spawn(&g, SP_BOSS, "big bad boss", 120)->y_q8 = 40 << 8;
    game_char(&g, 'c');                       /* lock target for bracket path */

    render_fx_t fx = { .laser_x = 60, .laser_y = 30, .laser_ttl = 2, .flash_ttl = 0 };
    render_game(&g, &fx);
    ASSERT_TRUE(nonzero_px(buf) > 2000);      /* aliens + HUD drew something */

    /* every screen renders without crashing */
    hs_table_t hs; hs_clear(&hs);
    hs_insert(&hs, "AAA", 1000, 22);
    render_title(&hs, 0);       ASSERT_TRUE(nonzero_px(buf) > 500);
    render_title(&hs, 99);
    render_level_intro(5);      ASSERT_TRUE(nonzero_px(buf) > 500);
    render_game_over(&g);       ASSERT_TRUE(nonzero_px(buf) > 500);
    render_initials("AB", 2, 12345);
    render_hiscores(&hs);       ASSERT_TRUE(nonzero_px(buf) > 500);

    /* flash overlay path */
    fx.flash_ttl = 2;
    render_game(&g, &fx);
    free(buf);
    TEST_RETURN();
}
