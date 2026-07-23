#include "sfx.h"
#include <string.h>

static uint32_t hz_inc(unsigned hz)
{
    return (uint32_t)(((uint64_t)hz << 32) / SFX_RATE);
}
static uint32_t ms_samples(unsigned ms) { return (uint32_t)ms * SFX_RATE / 1000; }

static void voice_square(sfx_voice_t *v, unsigned hz, unsigned ms, int amp)
{
    v->kind = 1;
    v->inc = hz_inc(hz);
    v->phase = 0;
    v->left = ms_samples(ms);
    v->amp = amp;
    v->decay = v->left ? amp / (int32_t)v->left : amp;
    v->seq = 0; v->looping = 0;
}
static void voice_noise(sfx_voice_t *v, unsigned ms, int amp)
{
    v->kind = 2;
    v->lfsr = 0xACE1;
    v->left = ms_samples(ms);
    v->amp = amp;
    v->decay = v->left ? amp / (int32_t)v->left : amp;
    v->seq = 0; v->looping = 0;
}
static void seq_step(sfx_voice_t *v)
{
    unsigned hz = v->seq[v->seq_i * 2], ms = v->seq[v->seq_i * 2 + 1];
    if (!hz && !ms) {
        if (!v->looping) { v->kind = 0; return; }
        v->seq_i = 0;
        hz = v->seq[0]; ms = v->seq[1];
    }
    v->inc = hz_inc(hz);
    v->left = ms_samples(ms);
    v->amp = 7000;
    v->decay = 0;                       /* sequences hold level per step */
    v->seq_i++;
}
static void voice_seq(sfx_voice_t *v, const uint16_t *seq, int looping)
{
    v->kind = 1;
    v->phase = 0;
    v->seq = seq; v->seq_i = 0; v->looping = (uint8_t)looping;
    seq_step(v);
}

static const uint16_t k_levelup[]  = { 523,80, 659,80, 784,80, 1047,200, 0,0 };
static const uint16_t k_gameover[] = { 392,150, 330,150, 262,320, 0,0 };
static const uint16_t k_title[]    = { 131,180, 0,60, 131,90, 0,30,
                                       196,180, 0,60, 165,90, 0,30, 0,0 };

void sfx_init(sfx_t *s) { memset(s, 0, sizeof *s); }

void sfx_trigger(sfx_t *s, sfx_id id, int arg)
{
    switch (id) {
    case SFX_ZAP:       voice_square(&s->v[0], 500 + 150 * (unsigned)arg, 60, 8000); break;
    case SFX_WRONG:     voice_square(&s->v[0], 130, 120, 8000); break;
    case SFX_KILL:      voice_noise(&s->v[1], 150, 9000); break;
    case SFX_BOSS_BOOM: voice_noise(&s->v[1], 420, 12000); break;
    case SFX_BOMB:      voice_noise(&s->v[1], 500, 12000); break;
    case SFX_LEVELUP:   voice_seq(&s->v[2], k_levelup, 0); break;
    case SFX_GAMEOVER:  voice_seq(&s->v[2], k_gameover, 0); break;
    case SFX_TITLE_LOOP:voice_seq(&s->v[2], k_title, 1); break;
    case SFX_LOOP_STOP: s->v[2].kind = 0; break;
    }
}

void sfx_render(sfx_t *s, int16_t *out, int n)
{
    for (int i = 0; i < n; i++) {
        int32_t mix = 0;
        for (int vi = 0; vi < SFX_VOICES; vi++) {
            sfx_voice_t *v = &s->v[vi];
            if (!v->kind) continue;
            if (v->kind == 1) {                    /* square */
                if (v->inc) {                      /* freq 0 = rest step */
                    mix += (v->phase & 0x80000000u) ? v->amp : -v->amp;
                    v->phase += v->inc;
                }
            } else {                               /* noise */
                unsigned bit = ((v->lfsr >> 0) ^ (v->lfsr >> 2) ^
                                (v->lfsr >> 3) ^ (v->lfsr >> 5)) & 1u;
                v->lfsr = (uint16_t)((v->lfsr >> 1) | (bit << 15));
                mix += (v->lfsr & 1) ? v->amp : -v->amp;
            }
            if (v->decay) { v->amp -= v->decay; if (v->amp < 0) v->amp = 0; }
            if (v->left && --v->left == 0) {
                if (v->seq) seq_step(v);
                else v->kind = 0;
            }
        }
        if (mix > 32767) mix = 32767;
        if (mix < -32768) mix = -32768;
        out[i] = (int16_t)mix;
    }
}
