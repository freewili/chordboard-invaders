#include "test_util.h"
#include "game.h"
#include "words.h"
#include <string.h>

static game_event_t last_of(game_t *g, game_event_type t, int *count) {
    game_event_t e, found = { GE_NONE, 0 };
    if (count) *count = 0;
    while (game_next_event(g, &e))
        if (e.type == t) { found = e; if (count) (*count)++; }
    return found;
}
static void type_word(game_t *g, const char *w) {
    for (const char *p = w; *p; p++) game_char(g, *p);
}

int main(void) {
    game_t g; int n;
    words_init(1); game_init(&g, 1); game_start(&g);
    g.spawn_left = 0;   /* manual spawns only */

    /* lock-on: first matching letter targets the alien and zaps */
    alien_t *a = game_force_spawn(&g, SP_DRIFTER, "cat", 50);
    game_char(&g, 'c');
    ASSERT_EQ(g.target, (int)(a - g.aliens));
    ASSERT_EQ(a->typed, 1);
    game_event_t z = last_of(&g, GE_ZAP, &n);
    ASSERT_EQ(n, 1); ASSERT_EQ(z.arg, 1);

    /* zap anchor captured at event time: alien center-bottom */
    ASSERT_EQ(g.zap_x_px, (a->x_q8 >> 8) + a->w_px / 2);
    ASSERT_EQ(g.zap_y_px, (a->y_q8 >> 8) + a->h_px);

    /* wrong letter: streak broken, target unlocked, event */
    game_char(&g, 'x');
    ASSERT_EQ(g.target, -1);
    last_of(&g, GE_WRONG, &n); ASSERT_EQ(n, 1);
    ASSERT_EQ(g.streak_mult, 1);
    ASSERT_EQ((int)g.chars_wrong, 1);

    /* re-lock and finish: kill + score 10*3*100%*1 = 30 */
    game_char(&g, 'c');           /* word restarts? no - typed stays at 1 */
    ASSERT_EQ(g.target, -1);      /* 'c' is not the next letter ('a' is) */
    last_of(&g, GE_WRONG, &n); ASSERT_EQ(n, 1);
    game_char(&g, 'a');           /* matches aliens[0] at typed=1 */
    ASSERT_EQ(g.target, (int)(a - g.aliens));
    game_char(&g, 't');
    ASSERT_TRUE(!a->active);
    last_of(&g, GE_KILL, &n); ASSERT_EQ(n, 1);
    ASSERT_EQ((int)g.score, 30);
    ASSERT_EQ(g.flawless_words, 0);   /* word had misses */

    /* clean words grow the streak: mult = min(8, 1+flawless) */
    for (int w = 0; w < 9; w++) {
        alien_t *b = game_force_spawn(&g, SP_DRIFTER, "dog", 60);
        ASSERT_TRUE(b != 0);
        type_word(&g, "dog");
        ASSERT_TRUE(!b->active);
    }
    ASSERT_EQ(g.streak_mult, 8);      /* capped */
    ASSERT_EQ(g.flawless_words, 9);

    /* ambiguity: lowest (largest y) matching alien wins the lock */
    game_t g2;
    words_init(2); game_init(&g2, 2); game_start(&g2);
    g2.spawn_left = 0;
    alien_t *hi = game_force_spawn(&g2, SP_DRIFTER, "map", 40);
    alien_t *lo = game_force_spawn(&g2, SP_DRIFTER, "mud", 200);
    hi->y_q8 = 20 << 8; lo->y_q8 = 120 << 8;
    game_char(&g2, 'm');
    ASSERT_EQ(g2.target, (int)(lo - g2.aliens));

    /* shielded: full word twice; shield pop refreshes the word */
    game_t g3;
    words_init(3); game_init(&g3, 3); game_start(&g3);
    g3.spawn_left = 0;
    alien_t *s = game_force_spawn(&g3, SP_SHIELDED, "bat", 80);
    type_word(&g3, "bat");
    ASSERT_TRUE(s->active);           /* survived: shield popped */
    ASSERT_TRUE(!s->shield);
    ASSERT_EQ(s->typed, 0);
    ASSERT_TRUE(strlen(s->word) > 0);
    last_of(&g3, GE_SHIELD_POP, &n); ASSERT_EQ(n, 1);
    type_word(&g3, s->word);
    ASSERT_TRUE(!s->active);
    TEST_RETURN();
}
