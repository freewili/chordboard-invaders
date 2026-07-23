// words.h - baked-in word lists + picker. All content lowercase a-y (no 'z',
// not typeable on the fw2kb LOWER page); phrases may contain spaces.
// This interface is the swap point for a future adaptive picker.
#ifndef WORDS_H
#define WORDS_H
#include <stdint.h>

typedef enum { WB_LETTERS = 0, WB_SHORT, WB_LONG, WB_PHRASE, WB_COUNT } word_bucket;

void        words_init(uint32_t seed);
const char *words_pick(word_bucket b);
int         words_count(word_bucket b);

#endif
