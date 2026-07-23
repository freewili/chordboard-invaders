#include "aliens.h"

static const alien_def_t k_defs[SP_COUNT] = {
    [SP_DRIFTER] = {
        .art = { { " /^\\ ", "(o o)", " )~( " },
                 { " /^\\ ", "(o o)", " (~) " } },
        .rows = 3, .cols = 5, .r = 0, .g = 255, .b = 80,
        .speed_pct = 100, .score_pct = 100, .min_level = 1, .weight = 50 },
    [SP_ZIGZAG] = {
        .art = { { "{o.o}", "/|_|\\" },
                 { "{o.o}", "\\|_|/" } },
        .rows = 2, .cols = 5, .r = 0, .g = 220, .b = 220,
        .speed_pct = 120, .score_pct = 120, .min_level = 2, .weight = 30 },
    [SP_DIVER] = {
        .art = { { " ^ ", "|||", " W " },
                 { " ^ ", "|||", " M " } },
        .rows = 3, .cols = 3, .r = 255, .g = 60, .b = 255,
        .speed_pct = 220, .score_pct = 200, .min_level = 5, .weight = 10 },
    [SP_SHIELDED] = {
        .art = { { "[###]", "(o_o)", "[###]" },
                 { "[###]", "(o-o)", "[###]" } },
        .rows = 3, .cols = 5, .r = 255, .g = 150, .b = 0,
        .speed_pct = 70, .score_pct = 180, .min_level = 4, .weight = 20 },
    [SP_BOSS] = {
        .art = { { " /MMMMMMM\\ ", "|  O   O  |", " \\_______/ ", "  v v v v  " },
                 { " /MMMMMMM\\ ", "|  -   -  |", " \\_______/ ", "  w w w w  " } },
        .rows = 4, .cols = 11, .r = 255, .g = 40, .b = 40,
        .speed_pct = 25, .score_pct = 300, .min_level = 1, .weight = 0 },
};

const alien_def_t *alien_def(species_t sp)
{
    return (sp >= 0 && sp < SP_COUNT) ? &k_defs[sp] : 0;
}

species_t alien_choose(int level, uint32_t rnd)
{
    int total = 0;
    for (int sp = 0; sp < SP_BOSS; sp++)
        if (k_defs[sp].min_level <= level) total += k_defs[sp].weight;
    int pick = (int)(rnd % (uint32_t)total);
    for (int sp = 0; sp < SP_BOSS; sp++) {
        if (k_defs[sp].min_level > level) continue;
        pick -= k_defs[sp].weight;
        if (pick < 0) return (species_t)sp;
    }
    return SP_DRIFTER;
}
