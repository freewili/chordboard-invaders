#include "words.h"

static const char *k_letters[] = {
    "a","b","c","d","e","f","g","h","i","j","k","l","m",
    "n","o","p","q","r","s","t","u","v","w","x","y" };

static const char *k_short[] = {
    "cat","dog","sun","map","key","run","jet","fox","owl","bat",
    "ray","ice","ant","bee","cow","egg","fig","gum","hat","ink",
    "jar","kit","log","mud","net","oak","pig","sky","tin","van",
    "wax","yak","star","moon","fire","wind","leaf","rain","snow","bolt",
    "gear","wire","code","byte","chip","ship","warp","beam" };

static const char *k_long[] = {
    "planet","rocket","galaxy","cosmic","photon","meteor","gravity","stellar",
    "nebula","quasar","plasma","asteroid","starship","invader","defense","capture",
    "blaster","cannon","charge","combat","crater","docking","engine","fusion",
    "launch","module","orbits","payload","reactor","saucer","shields","thruster",
    "vector","voyage","weapons","airlock" };

static const char *k_phrase[] = {
    "the quick brown fox",
    "type or be typed",
    "aliens hate typos",
    "chord your way out",
    "fast fingers win",
    "the mother ship arrives",
    "words are your weapon",
    "the final wave is here",
    "press it like you mean it",
    "boss battle commence" };

static const char **k_lists[WB_COUNT] = { k_letters, k_short, k_long, k_phrase };
static const int k_counts[WB_COUNT] = {
    sizeof k_letters / sizeof *k_letters,
    sizeof k_short   / sizeof *k_short,
    sizeof k_long    / sizeof *k_long,
    sizeof k_phrase  / sizeof *k_phrase };

static uint32_t s_rng = 1;
static int s_prev[WB_COUNT] = { -1, -1, -1, -1 };

static uint32_t next_rnd(void)
{
    s_rng ^= s_rng << 13;
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    return s_rng;
}

void words_init(uint32_t seed)
{
    s_rng = seed ? seed : 1;
    for (int i = 0; i < WB_COUNT; i++) s_prev[i] = -1;
}

int words_count(word_bucket b) { return k_counts[b]; }

const char *words_pick(word_bucket b)
{
    int n = k_counts[b];
    int i = (int)(next_rnd() % (uint32_t)n);
    if (n > 1 && i == s_prev[b]) i = (i + 1) % n;
    s_prev[b] = i;
    return k_lists[b][i];
}
