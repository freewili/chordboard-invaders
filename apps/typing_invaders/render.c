#include "render.h"
#include "fb.h"
#include "hints.h"
#include "aliens.h"
#include "display/st7796.h"
#include <string.h>
#include <stdio.h>

#define CELL_W 12
#define CELL_H 16
#define HUD_Y  296

/* fw2kb keycap colors (from the FW2 firmware soft menu) for hint dots */
static const uint8_t k_btn_rgb[5][3] = {
    { 214, 210, 214 },   /* gray   */
    { 255, 227,  49 },   /* yellow */
    {  60, 160,  30 },   /* green (brightened from #104100 for visibility) */
    {  40,  90, 255 },   /* blue (brightened from #001cc5) */
    { 220,  40,  90 },   /* red (brightened from #84003a) */
};

static uint16_t btn_col(int i)
{
    return fb_rgb(k_btn_rgb[i][0], k_btn_rgb[i][1], k_btn_rgb[i][2]);
}

static void draw_num(int x, int y, int scale, uint16_t fg, uint16_t bg,
                     const char *label, uint32_t v)
{
    char s[24];
    snprintf(s, sizeof s, "%s%u", label, (unsigned)v);
    fb_draw_text(x, y, scale, fg, bg, s);
}

static void draw_alien(const game_t *g, const alien_t *a, int idx)
{
    const alien_def_t *d = alien_def(a->sp);
    int x = a->x_q8 >> 8, y = a->y_q8 >> 8;
    uint16_t col = fb_rgb(d->r, d->g, d->b);
    int frame = (int)((g->ticks >> 3) & 1);

    for (int r = 0; r < d->rows; r++)
        fb_draw_text_t(x, y + r * CELL_H, 2, col, d->art[frame][r]);

    /* targeting bracket */
    if (g->target == idx) {
        uint16_t w = fb_rgb(255, 255, 255);
        fb_fill_rect(x - 4, y, 2, d->rows * CELL_H, w);
        fb_fill_rect(x + d->cols * CELL_W + 2, y, 2, d->rows * CELL_H, w);
    }

    /* word banner: typed dim, next bright, rest mid; hint dots per mode */
    int by = y + d->rows * CELL_H + 2;
    int wlen = (int)strlen(a->word);
    int bx = x + (d->cols * CELL_W - wlen * CELL_W) / 2;
    if (bx < 0) bx = 0;
    for (int i = 0; i < wlen; i++) {
        char cs[2] = { a->word[i], 0 };
        uint16_t fg = i < a->typed ? fb_rgb(90, 90, 90)
                    : i == a->typed ? fb_rgb(255, 255, 255)
                    : fb_rgb(190, 190, 190);
        fb_draw_text_t(bx + i * CELL_W, by, 2, fg, cs);
        int show = game_hint_all(g)
                || (game_hint_next(g) && i == a->typed)
                || game_hint_mercy(g, a->word[i]);
        int b1, b2;
        if (show && i >= a->typed && hints_chord(a->word[i], &b1, &b2)) {
            fb_fill_rect(bx + i * CELL_W + 1, by + CELL_H, 4, 4, btn_col(b1));
            fb_fill_rect(bx + i * CELL_W + 7, by + CELL_H, 4, 4, btn_col(b2));
        }
    }

    /* shielded: draw a shield tick above; boss: health bar */
    if (a->sp == SP_SHIELDED && a->shield)
        fb_draw_text_t(x, y - 10, 1, fb_rgb(255, 200, 0), "=SHIELD=");
    if (a->sp == SP_BOSS) {
        int total = wlen ? wlen : 1;
        int left = total - a->typed;
        int bw = d->cols * CELL_W;
        fb_fill_rect(x, y - 8, bw, 5, fb_rgb(60, 0, 0));
        fb_fill_rect(x, y - 8, bw * left / total, 5, fb_rgb(255, 40, 40));
    }
}

void render_game(const game_t *g, const render_fx_t *fx)
{
    if (fx->flash_ttl > 0) {                   /* smart-bomb whiteout frame */
        fb_clear(fb_rgb(255, 255, 255));
        return;
    }
    fb_clear(0);

    /* defense line + cannon */
    fb_fill_rect(0, GAME_DEFENSE_Y, GAME_FIELD_W, 1, fb_rgb(80, 80, 120));
    fb_draw_text_t(240 - 2 * CELL_W + CELL_W / 2, GAME_DEFENSE_Y + 2, 2,
                   fb_rgb(120, 255, 120), "/_A_\\");

    /* laser beam: alien center-bottom straight down to the defense line */
    if (fx->laser_ttl > 0 && fx->laser_y >= 0)
        fb_fill_rect(fx->laser_x - 1, fx->laser_y, 2,
                     GAME_DEFENSE_Y - fx->laser_y, fb_rgb(180, 255, 180));

    for (int i = 0; i < GAME_MAX_ALIENS; i++)
        if (g->aliens[i].active) draw_alien(g, &g->aliens[i], i);

    /* HUD strip */
    uint16_t hfg = fb_rgb(200, 200, 255);
    draw_num(4, HUD_Y, 2, hfg, 0, "", g->score);
    draw_num(150, HUD_Y, 2, fb_rgb(255, 220, 100), 0, "X", (uint32_t)g->streak_mult);
    draw_num(210, HUD_Y, 2, hfg, 0, "WPM ", (uint32_t)game_wpm(g));
    draw_num(330, HUD_Y, 2, hfg, 0, "LV", (uint32_t)g->level);
    for (int i = 0; i < g->lives; i++)   /* font5x7 covers 0x20-0x7E only */
        fb_draw_text_t(400 + i * 14, HUD_Y, 2, fb_rgb(255, 60, 60), "*");
    if (g->bomb_armed)
        fb_draw_text_t(450, HUD_Y, 2, fb_rgb(255, 255, 100), "@");
}

void render_title(const hs_table_t *hs, uint32_t frame)
{
    fb_clear(fb_rgb(0, 0, 16));
    fb_draw_text(96, 60, 4, fb_rgb(120, 255, 120), fb_rgb(0, 0, 16), "TYPING");
    fb_draw_text(72, 100, 4, fb_rgb(255, 80, 80), fb_rgb(0, 0, 16), "INVADERS");
    if ((frame >> 4) & 1)
        fb_draw_text(108, 180, 2, fb_rgb(255, 255, 255), fb_rgb(0, 0, 16),
                     "PRESS ANY CHORD TO START");
    char line[48];
    if (hs->count > 0)
        snprintf(line, sizeof line, "HI %s %u  BEST %u WPM",
                 hs->e[0].initials, (unsigned)hs->e[0].score,
                 (unsigned)hs->best_wpm);
    else
        snprintf(line, sizeof line, "NO SCORES YET - BE FIRST");
    fb_draw_text(60, 240, 2, fb_rgb(160, 160, 200), fb_rgb(0, 0, 16), line);
}

void render_level_intro(int level)
{
    fb_clear(0);
    char s[24];
    snprintf(s, sizeof s, "WAVE %d", level);
    fb_draw_text(160, 120, 4, fb_rgb(255, 255, 255), 0, s);
    const char *sub = game_is_boss_level(level) ? "!! BOSS INCOMING !!"
                    : level == 4 ? "HINTS FADE - NEXT LETTER ONLY"
                    : level == 7 ? "NO MORE HINTS - YOU KNOW THIS"
                    : "TYPE TO DESTROY";
    fb_draw_text(90, 170, 2, fb_rgb(255, 220, 100), 0, sub);
}

void render_game_over(const game_t *g)
{
    fb_clear(fb_rgb(24, 0, 0));
    fb_draw_text(110, 60, 4, fb_rgb(255, 80, 80), fb_rgb(24, 0, 0), "GAME OVER");
    draw_num(120, 130, 2, fb_rgb(255, 255, 255), fb_rgb(24, 0, 0),
             "SCORE ", g->score);
    draw_num(120, 156, 2, fb_rgb(255, 255, 255), fb_rgb(24, 0, 0),
             "AVG WPM ", (uint32_t)game_avg_wpm(g));
    draw_num(120, 182, 2, fb_rgb(255, 255, 255), fb_rgb(24, 0, 0),
             "ACCURACY ", (uint32_t)game_accuracy_pct(g));
    draw_num(120, 208, 2, fb_rgb(255, 255, 255), fb_rgb(24, 0, 0),
             "WAVE ", (uint32_t)g->level);
}

void render_initials(const char initials[4], int pos, uint32_t score)
{
    fb_clear(fb_rgb(0, 0, 24));
    fb_draw_text(84, 60, 2, fb_rgb(255, 255, 255), fb_rgb(0, 0, 24),
                 "NEW HIGH SCORE - ENTER INITIALS");
    draw_num(180, 90, 2, fb_rgb(255, 220, 100), fb_rgb(0, 0, 24), "", score);
    for (int i = 0; i < 3; i++) {
        int x = 180 + i * 48;
        char cs[2] = { initials[i] ? initials[i] : '_', 0 };
        uint16_t fg = i == pos ? fb_rgb(255, 255, 100) : fb_rgb(255, 255, 255);
        fb_draw_text(x, 140, 4, fg, fb_rgb(0, 0, 24), cs);
        fb_fill_rect(x, 176, 24, 2, fg);
    }
    fb_draw_text(96, 230, 1, fb_rgb(140, 140, 180), fb_rgb(0, 0, 24),
                 "chords type - tap top of screen = backspace");
}

void render_hiscores(const hs_table_t *hs)
{
    fb_clear(fb_rgb(0, 0, 16));
    fb_draw_text(140, 20, 3, fb_rgb(120, 255, 120), fb_rgb(0, 0, 16), "HALL OF FAME");
    for (int i = 0; i < hs->count; i++) {
        char line[40];
        snprintf(line, sizeof line, "%2d %s %8u %3u WPM", i + 1,
                 hs->e[i].initials, (unsigned)hs->e[i].score,
                 (unsigned)hs->e[i].wpm);
        fb_draw_text(90, 60 + i * 20, 2, fb_rgb(220, 220, 220), fb_rgb(0, 0, 16), line);
    }
    draw_num(150, 270, 2, fb_rgb(255, 220, 100), fb_rgb(0, 0, 16),
             "BEST WPM ", hs->best_wpm);
}
