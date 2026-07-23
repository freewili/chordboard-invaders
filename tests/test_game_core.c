#include "test_util.h"
#include "game.h"
#include "words.h"
#include <string.h>

static int active_count(const game_t *g) {
    int n = 0;
    for (int i = 0; i < GAME_MAX_ALIENS; i++) n += g->aliens[i].active;
    return n;
}
static int drain_count(game_t *g, game_event_type t) {
    game_event_t e; int n = 0;
    while (game_next_event(g, &e)) if (e.type == t) n++;
    return n;
}

int main(void) {
    game_t g;
    words_init(42);
    game_init(&g, 42);
    game_start(&g);
    ASSERT_EQ(g.level, 1); ASSERT_EQ(g.lives, GAME_LIVES);
    ASSERT_TRUE(g.bomb_armed); ASSERT_TRUE(g.spawn_left > 0);

    /* aliens appear over time and respect the concurrency cap. 300 ticks
     * (10 s) is long enough to hit the cap but too short for any alien to
     * reach the defense line (~456 ticks at level-1 speed). */
    for (int t = 0; t < 300; t++) game_tick(&g);
    ASSERT_TRUE(active_count(&g) >= 1);
    ASSERT_TRUE(active_count(&g) <= 3);   /* level 1 cap = 2 + (1+1)/2 = 3 */

    /* falling: y grows each tick */
    int idx = -1;
    for (int i = 0; i < GAME_MAX_ALIENS; i++) if (g.aliens[i].active) { idx = i; break; }
    int32_t y0 = g.aliens[idx].y_q8;
    game_tick(&g);
    ASSERT_TRUE(g.aliens[idx].y_q8 > y0);

    /* crossing the defense line costs a life */
    (void)drain_count(&g, GE_NONE);   /* clear ring */
    g.aliens[idx].y_q8 = (GAME_DEFENSE_Y - 1) << 8;
    game_tick(&g);
    ASSERT_TRUE(!g.aliens[idx].active);
    ASSERT_EQ(g.lives, GAME_LIVES - 1);

    /* forcing all remaining lives away ends the game */
    game_t g2;
    game_init(&g2, 7); game_start(&g2);
    for (int life = 0; life < GAME_LIVES; life++) {
        alien_t *a = game_force_spawn(&g2, SP_DRIFTER, "cat", 100);
        ASSERT_TRUE(a != 0);
        a->y_q8 = (GAME_DEFENSE_Y - 1) << 8;
        game_tick(&g2);
    }
    ASSERT_TRUE(g2.game_over);
    ASSERT_EQ(drain_count(&g2, GE_GAME_OVER), 1);

    /* wave clear -> level up: exhaust spawns, kill everything via state */
    game_t g3;
    game_init(&g3, 9); game_start(&g3);
    g3.spawn_left = 0;
    for (int i = 0; i < GAME_MAX_ALIENS; i++) g3.aliens[i].active = false;
    game_tick(&g3);
    ASSERT_EQ(g3.level, 2);
    ASSERT_EQ(drain_count(&g3, GE_LEVEL_UP), 1);
    ASSERT_TRUE(g3.bomb_armed);

    /* boss level flag */
    ASSERT_TRUE(game_is_boss_level(5));
    ASSERT_TRUE(game_is_boss_level(10));
    ASSERT_TRUE(!game_is_boss_level(4));
    TEST_RETURN();
}
