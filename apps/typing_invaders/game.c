#include "game.h"
#include "words.h"
#include <string.h>

#define CELL_W 12               /* render cell: 6*2 px */
#define CELL_H 16               /* 8*2 px */
#define BANNER_H 18             /* word banner strip under the art */

/* 32-step sway table, amplitude 24 px (zigzag species) */
static const int8_t k_sway[32] = {
      0,  5,  9, 13, 17, 20, 22, 24, 24, 24, 22, 20, 17, 13,  9,  5,
      0, -5, -9,-13,-17,-20,-22,-24,-24,-24,-22,-20,-17,-13, -9, -5 };

static uint32_t next_rnd(game_t *g)
{
    g->rng ^= g->rng << 13;
    g->rng ^= g->rng >> 17;
    g->rng ^= g->rng << 5;
    return g->rng;
}

void game_push_event(game_t *g, game_event_type t, int32_t arg)
{
    if (g->ev_count == GAME_EVENT_RING) {            /* drop oldest */
        g->ev_head = (g->ev_head + 1) % GAME_EVENT_RING;
        g->ev_count--;
    }
    int tail = (g->ev_head + g->ev_count) % GAME_EVENT_RING;
    g->ev[tail].type = t;
    g->ev[tail].arg = arg;
    g->ev_count++;
}

bool game_next_event(game_t *g, game_event_t *out)
{
    if (!g->ev_count) return false;
    *out = g->ev[g->ev_head];
    g->ev_head = (g->ev_head + 1) % GAME_EVENT_RING;
    g->ev_count--;
    return true;
}

bool game_is_boss_level(int level) { return level % 5 == 0; }

/* ---- level tuning ---- */
static int level_budget(int level)
{
    if (game_is_boss_level(level)) return 1;
    int n = 6 + 2 * level;
    return n > 20 ? 20 : n;
}
static int level_concurrent(int level)
{
    if (game_is_boss_level(level)) return 1;
    int n = 2 + (level + 1) / 2;
    return n > 6 ? 6 : n;
}
static int32_t level_base_vy_q8(int level)
{
    int32_t v = 118 + 12 * (level - 1);
    return v > 512 ? 512 : v;
}
static word_bucket level_bucket(int level)
{
    if (level <= 2) return WB_LETTERS;
    if (level <= 5) return WB_SHORT;
    return WB_LONG;
}
static int spawn_gap_ticks(game_t *g)
{
    int base = 45 - 2 * g->level;
    if (base < 15) base = 15;
    return base + (int)(next_rnd(g) % 21) - 10;      /* +/- 10 tick jitter */
}

static void wave_setup(game_t *g)
{
    g->spawn_left = level_budget(g->level);
    g->spawn_cooldown = 20;
    g->bomb_armed = true;
}

void game_init(game_t *g, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    g->rng = seed ? seed : 1;
    g->target = -1;
}

void game_start(game_t *g)
{
    uint32_t rng = g->rng;
    memset(g, 0, sizeof *g);
    g->rng = rng;
    g->level = 1;
    g->lives = GAME_LIVES;
    g->streak_mult = 1;
    g->target = -1;
    wave_setup(g);
}

alien_t *game_force_spawn(game_t *g, species_t sp, const char *word, int x_px)
{
    int idx = -1;
    for (int i = 0; i < GAME_MAX_ALIENS; i++)
        if (!g->aliens[i].active) { idx = i; break; }
    if (idx < 0) return 0;

    alien_t *a = &g->aliens[idx];
    const alien_def_t *d = alien_def(sp);
    memset(a, 0, sizeof *a);
    a->active = true;
    a->sp = sp;
    if (!word) {
        word_bucket b = (sp == SP_BOSS) ? WB_PHRASE
                       : (sp == SP_DIVER) ? WB_SHORT
                       : level_bucket(g->level);
        word = words_pick(b);
    }
    strncpy(a->word, word, GAME_WORD_MAX - 1);
    int wlen = (int)strlen(a->word);
    int cells = d->cols > wlen ? d->cols : wlen;
    a->w_px = cells * CELL_W;
    a->h_px = d->rows * CELL_H + BANNER_H;
    a->shield = (sp == SP_SHIELDED);
    a->vy_q8 = level_base_vy_q8(g->level) * d->speed_pct / 100;
    int max_x = GAME_FIELD_W - a->w_px - 16;
    if (x_px < 8) x_px = 8;
    if (x_px > max_x) x_px = max_x;
    a->x_q8 = a->home_x_q8 = x_px << 8;
    a->y_q8 = 0;
    return a;
}

static void spawn_from_wave(game_t *g)
{
    species_t sp = game_is_boss_level(g->level) ? SP_BOSS
                                                : alien_choose(g->level, next_rnd(g));
    int x = 8 + (int)(next_rnd(g) % 400);
    if (game_force_spawn(g, sp, 0, x)) {
        g->spawn_left--;
        g->spawn_cooldown = spawn_gap_ticks(g);
    }
}

static void alien_deactivate(game_t *g, int i)
{
    g->aliens[i].active = false;
    if (g->target == i) g->target = -1;
}

void game_tick(game_t *g)
{
    if (g->game_over) return;
    g->ticks++;

    int active = 0;
    for (int i = 0; i < GAME_MAX_ALIENS; i++) active += g->aliens[i].active;

    if (g->spawn_left > 0 && active < level_concurrent(g->level)) {
        if (--g->spawn_cooldown <= 0) { spawn_from_wave(g); active++; }
    }

    for (int i = 0; i < GAME_MAX_ALIENS; i++) {
        alien_t *a = &g->aliens[i];
        if (!a->active) continue;
        a->y_q8 += a->vy_q8;
        if (a->sp == SP_ZIGZAG) {
            a->phase++;
            int32_t sway = (int32_t)k_sway[(a->phase >> 2) & 31] << 8;
            a->x_q8 = a->home_x_q8 + sway;
            if (a->x_q8 < 8 << 8) a->x_q8 = 8 << 8;
            int32_t max_x = (GAME_FIELD_W - a->w_px - 8) << 8;
            if (a->x_q8 > max_x) a->x_q8 = max_x;
        }
        if ((a->y_q8 >> 8) + a->h_px >= GAME_DEFENSE_Y) {
            alien_deactivate(g, i);
            g->lives--;
            game_push_event(g, GE_LIFE_LOST, g->lives);
            g->streak_mult = 1;
            g->flawless_words = 0;
            if (g->lives <= 0) {
                g->game_over = true;
                game_push_event(g, GE_GAME_OVER, 0);
                return;
            }
        }
    }

    active = 0;
    for (int i = 0; i < GAME_MAX_ALIENS; i++) active += g->aliens[i].active;
    if (g->spawn_left == 0 && active == 0) {
        g->level++;
        wave_setup(g);
        game_push_event(g, GE_LEVEL_UP, g->level);
    }
}

/* ---- Task 6 typing mechanics ---- */

static void stamp_char(game_t *g)
{
    g->chars_typed++;
    g->char_ticks[g->char_head] = g->ticks;
    g->char_head = (g->char_head + 1) % GAME_CHAR_RING;
}

static void mark_wrong(game_t *g, char expected)
{
    g->chars_wrong++;
    g->word_dirty = true;
    g->flawless_words = 0;
    g->streak_mult = 1;
    g->target = -1;
    if (expected) {
        if (expected == g->fumble_ch) g->fumble_n++;
        else { g->fumble_ch = expected; g->fumble_n = 1; }
    }
    game_push_event(g, GE_WRONG, 0);
}

static void score_kill(game_t *g, alien_t *a)
{
    int len = 0;
    for (const char *p = a->word; *p; p++) if (*p != ' ') len++;
    const alien_def_t *d = alien_def(a->sp);
    g->score += (uint32_t)(10 * len * d->score_pct / 100 * g->streak_mult);
}

static void advance_letter(game_t *g, alien_t *a)
{
    a->typed++;
    stamp_char(g);
    if (a->word[a->typed - 1] == g->fumble_ch) g->fumble_n = 0;  /* mercy clears */
    game_push_event(g, GE_ZAP, g->streak_mult);

    while (a->word[a->typed] == ' ') {            /* boss phrases auto-advance */
        a->typed++;
        int remaining = 0;
        for (const char *p = a->word + a->typed; *p; p++) remaining += (*p == ' ');
        game_push_event(g, GE_BOSS_CHUNK, remaining + 1);
    }

    if (a->word[a->typed] != '\0') return;

    /* word complete */
    if (a->sp == SP_SHIELDED && a->shield) {
        a->shield = false;
        a->typed = 0;
        strncpy(a->word, words_pick(level_bucket(g->level)), GAME_WORD_MAX - 1);
        a->word[GAME_WORD_MAX - 1] = '\0';
        g->word_dirty = false;         /* fresh word, fresh flawless chance */
        game_push_event(g, GE_SHIELD_POP, 0);
        return;
    }
    score_kill(g, a);
    if (a->sp == SP_BOSS) game_push_event(g, GE_BOSS_KILL, 0);
    game_push_event(g, GE_KILL, (int32_t)a->sp);
    int idx = (int)(a - g->aliens);
    alien_deactivate(g, idx);
    if (!g->word_dirty) {
        g->flawless_words++;
        g->streak_mult = 1 + g->flawless_words;
        if (g->streak_mult > 8) g->streak_mult = 8;
    }
    g->word_dirty = false;
}

void game_char(game_t *g, char c)
{
    if (g->game_over) return;
    if (g->target >= 0 && g->aliens[g->target].active) {
        alien_t *a = &g->aliens[g->target];
        if (a->word[a->typed] == c) { advance_letter(g, a); return; }
        mark_wrong(g, a->word[a->typed]);
        return;
    }
    /* no lock: target the lowest (deepest) alien whose next letter matches */
    int best = -1;
    for (int i = 0; i < GAME_MAX_ALIENS; i++) {
        alien_t *a = &g->aliens[i];
        if (!a->active || a->word[a->typed] != c) continue;
        if (best < 0 || a->y_q8 > g->aliens[best].y_q8) best = i;
    }
    if (best < 0) { mark_wrong(g, 0); return; }
    g->target = best;
    /* NOTE: word_dirty is NOT cleared here - a re-lock continues the same
     * word attempt, so an earlier miss still marks it dirty. It clears only
     * on kill and on shield pop. */
    advance_letter(g, &g->aliens[best]);
}

/* ---- Task 7 fill these in ---- */
void game_shake(game_t *g) { (void)g; }
int  game_wpm(const game_t *g) { (void)g; return 0; }
int  game_avg_wpm(const game_t *g) { (void)g; return 0; }
int  game_accuracy_pct(const game_t *g) { (void)g; return 100; }
bool game_hint_all(const game_t *g) { return g->level <= 3; }
bool game_hint_next(const game_t *g) { return g->level <= 6; }
bool game_hint_mercy(const game_t *g, char c) { (void)g; (void)c; return false; }
