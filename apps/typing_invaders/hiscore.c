#include "hiscore.h"
#include <string.h>

#define HS_MAGIC 0x54595031u   /* "TYP1" */

void hs_clear(hs_table_t *t) { memset(t, 0, sizeof *t); }

int hs_rank(const hs_table_t *t, uint32_t score)
{
    for (int i = 0; i < t->count; i++)
        if (score > t->e[i].score) return i;
    return t->count < HS_MAX ? t->count : -1;
}

void hs_insert(hs_table_t *t, const char *initials, uint32_t score, uint16_t wpm)
{
    int at = hs_rank(t, score);
    if (at < 0) return;
    int last = t->count < HS_MAX ? t->count : HS_MAX - 1;
    for (int i = last; i > at; i--) t->e[i] = t->e[i - 1];
    memset(&t->e[at], 0, sizeof t->e[at]);
    strncpy(t->e[at].initials, initials, 3);
    t->e[at].score = score;
    t->e[at].wpm = wpm;
    if (t->count < HS_MAX) t->count++;
}

void hs_note_wpm(hs_table_t *t, uint16_t wpm)
{
    if (wpm > t->best_wpm) t->best_wpm = wpm;
}

static uint32_t crc32(const uint8_t *p, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int b = 0; b < 8; b++)
            c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
    }
    return ~c;
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* layout: magic u32 | count u8 | best_wpm u16 | pad u8 |
 *         10 x (initials 4B | score u32 | wpm u16) = 100B | crc u32
 * total 8 + 100 + 4 = 112, zero-padded to HS_BLOB_SIZE. */
void hs_encode(const hs_table_t *t, uint8_t blob[HS_BLOB_SIZE])
{
    memset(blob, 0, HS_BLOB_SIZE);
    put32(blob, HS_MAGIC);
    blob[4] = (uint8_t)t->count;
    blob[5] = (uint8_t)t->best_wpm;
    blob[6] = (uint8_t)(t->best_wpm >> 8);
    uint8_t *p = blob + 8;
    for (int i = 0; i < HS_MAX; i++, p += 10) {
        memcpy(p, t->e[i].initials, 4);
        put32(p + 4, t->e[i].score);
        p[8] = (uint8_t)t->e[i].wpm;
        p[9] = (uint8_t)(t->e[i].wpm >> 8);
    }
    put32(blob + 108, crc32(blob, 108));
}

bool hs_decode(hs_table_t *t, const uint8_t blob[HS_BLOB_SIZE])
{
    if (get32(blob) != HS_MAGIC) return false;
    if (get32(blob + 108) != crc32(blob, 108)) return false;
    hs_clear(t);
    t->count = blob[4];
    if (t->count > HS_MAX) return false;
    t->best_wpm = (uint16_t)(blob[5] | (blob[6] << 8));
    const uint8_t *p = blob + 8;
    for (int i = 0; i < HS_MAX; i++, p += 10) {
        memcpy(t->e[i].initials, p, 4);
        t->e[i].initials[3] = '\0';
        t->e[i].score = get32(p + 4);
        t->e[i].wpm = (uint16_t)(p[8] | (p[9] << 8));
    }
    return true;
}
