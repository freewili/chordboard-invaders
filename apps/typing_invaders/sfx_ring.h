// sfx_ring.h - couples the pure sfx synth to the I2S DMA read-ring.
// An 8192-frame ring (SRAM - flash writes stall PSRAM) loops forever via the
// zero-CPU DMA read-ring (audio_i2s_duplex_play_loop), buffer aligned to its
// 32 KB size as the API requires; pump() renders ~100 ms ahead of a
// wall-clock playhead.
#ifndef SFX_RING_H
#define SFX_RING_H
#include "sfx.h"

void sfxring_init(sfx_t *s);     /* codec + i2s up, ring armed */
void sfxring_pump(void);         /* call every main-loop pass */
void sfxring_stop(void);         /* silence + DMA stopped (for flash writes) */
void sfxring_resume(void);
#endif
