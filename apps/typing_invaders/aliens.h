// aliens.h - species tables: 2-frame ASCII art, color, speed/score scaling,
// spawn gating. Pure data, host-testable.
#ifndef ALIENS_H
#define ALIENS_H
#include <stdint.h>

typedef enum {
    SP_DRIFTER = 0, SP_ZIGZAG, SP_DIVER, SP_SHIELDED, SP_BOSS, SP_COUNT
} species_t;

#define ALIEN_ART_ROWS 4

typedef struct {
    const char *art[2][ALIEN_ART_ROWS];  /* [frame][row]; rows..ALIEN_ART_ROWS-1 unused */
    int     rows, cols;                  /* art size in character cells */
    uint8_t r, g, b;                     /* species color */
    int     speed_pct;                   /* % of level base fall speed */
    int     score_pct;                   /* % of base word score */
    int     min_level;                   /* first level this species may spawn */
    int     weight;                      /* spawn weight (SP_BOSS: unused) */
} alien_def_t;

const alien_def_t *alien_def(species_t sp);
species_t alien_choose(int level, uint32_t rnd);   /* weighted non-boss pick */

#endif
