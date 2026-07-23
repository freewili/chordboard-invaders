// sfx.h - pure 2-oscillator chiptune synth at the codec rate. No hardware,
// no floats. Voices: v0 = player squares (zap/wrong), v1 = noise (booms),
// v2 = sequences (jingles + title loop).
#ifndef SFX_H
#define SFX_H
#include <stdint.h>

#define SFX_RATE 16009

typedef enum {
    SFX_ZAP = 0,      /* arg = streak multiplier 1..8 (pitch) */
    SFX_KILL,
    SFX_BOSS_BOOM,
    SFX_WRONG,
    SFX_LEVELUP,
    SFX_GAMEOVER,
    SFX_BOMB,
    SFX_TITLE_LOOP,
    SFX_LOOP_STOP,
} sfx_id;

#define SFX_VOICES 3

typedef struct {
    uint8_t  kind;            /* 0 off, 1 square, 2 noise */
    uint32_t phase, inc;
    int32_t  amp, decay;      /* linear envelope, amp units per sample */
    uint32_t left;            /* samples left in current step */
    const uint16_t *seq;      /* pairs (freq_hz, ms), terminated by 0,0 */
    int      seq_i;
    uint8_t  looping;
    uint16_t lfsr;
} sfx_voice_t;

typedef struct { sfx_voice_t v[SFX_VOICES]; } sfx_t;

void sfx_init(sfx_t *s);
void sfx_trigger(sfx_t *s, sfx_id id, int arg);
void sfx_render(sfx_t *s, int16_t *out, int n);

#endif
