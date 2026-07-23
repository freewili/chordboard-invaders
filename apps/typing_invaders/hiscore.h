// hiscore.h - top-10 score table + all-time best WPM, with a fixed-size
// checksummed blob codec for flash persistence. Pure, host-testable.
#ifndef HISCORE_H
#define HISCORE_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define HS_MAX 10
#define HS_BLOB_SIZE 128

typedef struct {
    char     initials[4];      /* 3 chars + NUL */
    uint32_t score;
    uint16_t wpm;
} hs_entry_t;

typedef struct {
    hs_entry_t e[HS_MAX];
    int        count;
    uint16_t   best_wpm;
} hs_table_t;

void   hs_clear(hs_table_t *t);
int    hs_rank(const hs_table_t *t, uint32_t score);  /* insert idx, -1 = no */
void   hs_insert(hs_table_t *t, const char *initials, uint32_t score, uint16_t wpm);
void   hs_note_wpm(hs_table_t *t, uint16_t wpm);
void   hs_encode(const hs_table_t *t, uint8_t blob[HS_BLOB_SIZE]);
bool   hs_decode(hs_table_t *t, const uint8_t blob[HS_BLOB_SIZE]);

#endif
