// render.h - draws all screens into the fb module's current buffer.
// Pure w.r.t. hardware (fb is a plain memory buffer); main.c flushes.
#ifndef RENDER_H
#define RENDER_H
#include <stdint.h>
#include "game.h"
#include "hiscore.h"

typedef struct {
    int laser_x, laser_y;   /* beam top point (alien center-bottom), px */
    int laser_ttl;          /* ticks left to draw the beam */
    int flash_ttl;          /* smart-bomb white flash */
} render_fx_t;

void render_game(const game_t *g, const render_fx_t *fx);
void render_title(const hs_table_t *hs, uint32_t frame);
void render_level_intro(int level);
void render_game_over(const game_t *g);
void render_initials(const char initials[4], int pos, uint32_t score);
void render_hiscores(const hs_table_t *hs);

/* Live chordboard bar (bottom 22 px): five keycap-colored buttons showing the
 * fw2kb labels — letter groups normally, single letters mid-chord. Drawn LAST,
 * over whatever the screen renderer left in that strip. */
void render_chordbar(const char *labels[5]);

#endif
