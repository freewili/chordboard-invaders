#include "test_util.h"
#include "game.h"
#include "words.h"
#include <string.h>

static int count_of(game_t *g, game_event_type t) {
    game_event_t e; int n = 0;
    while (game_next_event(g, &e)) if (e.type == t) n++;
    return n;
}

int main(void) {
    game_t g;
    words_init(5); game_init(&g, 5); game_start(&g);
    g.spawn_left = 0;

    /* boss phrase: spaces auto-advance, chunk events fire, kill at the end */
    alien_t *boss = game_force_spawn(&g, SP_BOSS, "big bad boss", 100);
    for (const char *p = "bigbadboss"; *p; p++) game_char(&g, *p);
    ASSERT_TRUE(!boss->active);
    ASSERT_EQ(count_of(&g, GE_BOSS_CHUNK), 2);   /* after "big", after "bad" */
    /* re-check kill events were emitted */
    /* (ring already drained by count_of; re-run scenario for BOSS_KILL) */
    alien_t *boss2 = game_force_spawn(&g, SP_BOSS, "go now", 100);
    for (const char *p = "gonow"; *p; p++) game_char(&g, *p);
    ASSERT_TRUE(!boss2->active);
    ASSERT_EQ(count_of(&g, GE_BOSS_KILL), 1);

    /* WPM: 25 correct chars in the window = 25/5 words / 15 s => 20 wpm */
    game_t g2;
    words_init(6); game_init(&g2, 6); game_start(&g2);
    g2.spawn_left = 0;
    for (int t = 0; t < GAME_WPM_WINDOW; t++) game_tick(&g2);  /* fill window */
    for (int w = 0; w < 5; w++) {
        alien_t *a = game_force_spawn(&g2, SP_DRIFTER, "cargo", 50);
        for (const char *p = "cargo"; *p; p++) game_char(&g2, *p);
        ASSERT_TRUE(!a->active);
    }
    ASSERT_EQ(game_wpm(&g2), 20);
    ASSERT_EQ(game_accuracy_pct(&g2), 100);
    game_char(&g2, 'q');                     /* one miss */
    ASSERT_EQ(game_accuracy_pct(&g2), 96);   /* 25/26 = 96% */
    ASSERT_TRUE(game_avg_wpm(&g2) > 0);

    /* smart bomb: kills everything for zero points, disarms until level-up */
    game_t g3;
    words_init(7); game_init(&g3, 7); game_start(&g3);
    g3.spawn_left = 0;
    game_force_spawn(&g3, SP_DRIFTER, "cat", 40);
    game_force_spawn(&g3, SP_ZIGZAG, "dog", 200);
    uint32_t score_before = g3.score;
    game_shake(&g3);
    ASSERT_EQ(count_of(&g3, GE_BOMB), 1);
    ASSERT_EQ((int)g3.score, (int)score_before);
    ASSERT_TRUE(!g3.bomb_armed);
    for (int i = 0; i < GAME_MAX_ALIENS; i++) ASSERT_TRUE(!g3.aliens[i].active);
    game_shake(&g3);                          /* disarmed: no event */
    ASSERT_EQ(count_of(&g3, GE_BOMB), 0);

    /* mercy: fumbling the same expected letter twice enables the hint */
    game_t gm;
    words_init(12); game_init(&gm, 12); game_start(&gm);
    gm.spawn_left = 0;
    game_force_spawn(&gm, SP_DRIFTER, "kev", 40);
    game_char(&gm, 'k');            /* lock, expected 'e' */
    game_char(&gm, 'x');            /* fumble 'e' #1 (unlocks) */
    game_char(&gm, 'k');            /* no match -> wrong, expected=0, uncounted */
    game_char(&gm, 'e');            /* re-lock+advance: clears the counter */
    ASSERT_TRUE(!game_hint_mercy(&gm, 'e'));
    game_char(&gm, 'x');            /* expected 'v': fumble 'v' #1 */
    game_char(&gm, 'v');            /* re-lock, typed=2 -> word done, kill */
    /* direct counter check (white box: two consecutive fumbles of one char) */
    gm.fumble_ch = 'q'; gm.fumble_n = 2;
    ASSERT_TRUE(game_hint_mercy(&gm, 'q'));
    ASSERT_TRUE(!game_hint_mercy(&gm, 'r'));
    gm.fumble_n = 1;
    ASSERT_TRUE(!game_hint_mercy(&gm, 'q'));
    TEST_RETURN();
}
