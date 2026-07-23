#include "sfx_ring.h"
#include "fw2.h"
#include "pico/stdlib.h"
#include <string.h>

#define RING_FRAMES 8192u        /* one stream chunk = 512 ms */

static uint32_t s_ring[RING_FRAMES] __attribute__((aligned(32768)));
static uint64_t s_t0;
static uint32_t s_written;
static sfx_t   *s_sfx;

void sfxring_init(sfx_t *s)
{
    s_sfx = s;
    codec_nau88c10_init();
    audio_i2s_duplex_init(16000);
    codec_nau88c10_dac_mute(false);
    memset(s_ring, 0, sizeof s_ring);
    audio_i2s_duplex_play_loop(s_ring, RING_FRAMES);
    s_t0 = time_us_64();
    s_written = 0;
}

void sfxring_pump(void)
{
    if (!s_sfx) return;
    uint64_t pos = (time_us_64() - s_t0) * SFX_RATE / 1000000ull;
    uint32_t target = (uint32_t)pos + SFX_RATE / 10;    /* 100 ms lead */
    while (s_written < target) {
        int16_t chunk[128];
        uint32_t want = target - s_written;
        int n = want > 128 ? 128 : (int)want;
        sfx_render(s_sfx, chunk, n);
        for (int i = 0; i < n; i++) {
            uint16_t u = (uint16_t)chunk[i];
            s_ring[(s_written + (uint32_t)i) % RING_FRAMES] =
                ((uint32_t)u << 16) | u;
        }
        s_written += (uint32_t)n;
    }
}

void sfxring_stop(void)
{
    audio_i2s_duplex_play_stop();
}

void sfxring_resume(void)
{
    memset(s_ring, 0, sizeof s_ring);
    audio_i2s_duplex_play_loop(s_ring, RING_FRAMES);
    s_t0 = time_us_64();
    s_written = 0;
}
