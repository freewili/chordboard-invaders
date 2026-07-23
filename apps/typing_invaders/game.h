// game.h - pure game rules for Typing Invaders. No hardware includes.
// Fixed timestep: one game_tick() == GAME_TICK_MS. Positions are 24.8 fixed
// point pixels. Consumers (render/sfx/led/haptic) read state + pop events.
#ifndef GAME_H
#define GAME_H
#include <stdbool.h>
#include <stdint.h>
#include "aliens.h"

#define GAME_TICK_MS        33
#define GAME_TICKS_PER_SEC  30
#define GAME_MAX_ALIENS     8
#define GAME_WORD_MAX       40
#define GAME_EVENT_RING     16
#define GAME_LIVES          3
#define GAME_FIELD_W        480
#define GAME_DEFENSE_Y      258     /* aliens crossing this line cost a life
                                       (field ends here; cannon+HUD+chord bar
                                       occupy 258..320) */
#define GAME_CHAR_RING      64
#define GAME_WPM_WINDOW     450     /* 15 s of ticks for the live WPM figure */

typedef enum {
    GE_NONE = 0,
    GE_ZAP,          /* correct letter; arg = streak multiplier 1..8 */
    GE_KILL,         /* alien destroyed by typing; arg = species_t */
    GE_WRONG,        /* wrong letter; arg = 0 */
    GE_SHIELD_POP,   /* shielded alien lost its shield; arg = 0 */
    GE_BOSS_CHUNK,   /* boss phrase word completed; arg = words remaining */
    GE_BOSS_KILL,    /* boss destroyed; arg = 0 */
    GE_LIFE_LOST,    /* arg = lives remaining */
    GE_LEVEL_UP,     /* wave cleared; arg = new level */
    GE_BOMB,         /* smart bomb fired; arg = aliens destroyed */
    GE_GAME_OVER,    /* arg = 0 (read score from game_t) */
} game_event_type;

typedef struct { game_event_type type; int32_t arg; } game_event_t;

typedef struct {
    bool      active;
    species_t sp;
    int32_t   x_q8, y_q8;       /* top-left, 24.8 fixed-point pixels */
    int32_t   home_x_q8;        /* sway centre (zigzag) */
    int32_t   vy_q8;            /* fall per tick */
    int32_t   phase;            /* sway phase, ticks */
    char      word[GAME_WORD_MAX];
    int       typed;            /* chars completed */
    bool      shield;           /* SP_SHIELDED: armor still up */
    int       w_px, h_px;       /* footprint incl. banner (set at spawn) */
} alien_t;

typedef struct {
    int      level;
    int      lives;
    uint32_t score;
    int      streak_mult;       /* 1..8 */
    int      flawless_words;
    bool     word_dirty;        /* current word had a miss */
    int      target;            /* locked alien index, -1 = none */
    bool     bomb_armed;
    int      spawn_left;
    int      spawn_cooldown;    /* ticks to next spawn */
    uint32_t ticks;
    uint32_t chars_typed;       /* correct chars, whole run */
    uint32_t chars_wrong;
    uint32_t char_ticks[GAME_CHAR_RING];  /* stamps of recent correct chars */
    int      char_head;
    char     fumble_ch;         /* mercy-hint tracking */
    int      fumble_n;
    bool     game_over;
    alien_t  aliens[GAME_MAX_ALIENS];
    game_event_t ev[GAME_EVENT_RING];
    int      ev_head, ev_count;
    int32_t  zap_x_px, zap_y_px;    /* laser anchor: set at each GE_ZAP push */
    uint32_t rng;
} game_t;

void game_init(game_t *g, uint32_t seed);
void game_start(game_t *g);                    /* new run at level 1 */
void game_tick(game_t *g);
void game_char(game_t *g, char c);             /* completed chord letter */
void game_shake(game_t *g);                    /* smart bomb request */
bool game_next_event(game_t *g, game_event_t *out);

/* exposed for internal reuse + tests: spawn a specific alien at column x_px
 * (word=NULL -> pick from the level's bucket). Returns NULL if field full. */
alien_t *game_force_spawn(game_t *g, species_t sp, const char *word, int x_px);

/* pure HUD queries */
int  game_wpm(const game_t *g);                /* rolling 15 s window */
int  game_avg_wpm(const game_t *g);            /* whole run */
int  game_accuracy_pct(const game_t *g);
bool game_hint_all(const game_t *g);           /* level <= 3: dots everywhere */
bool game_hint_next(const game_t *g);          /* level <= 6: dots on next char */
bool game_hint_mercy(const game_t *g, char c); /* fumbled-twice override */
bool game_is_boss_level(int level);

#endif
