#include "test_util.h"
#include "words.h"
#include "hints.h"
#include <string.h>

/* every word char must be typeable on the LOWER page (a-y), or a space in
 * phrases; no word may exceed 39 chars (game word buffer is 40). */
static void check_bucket(word_bucket b, int allow_space) {
    int a, s;
    for (int k = 0; k < 200; k++) {
        const char *w = words_pick(b);
        ASSERT_TRUE(w != 0);
        ASSERT_TRUE(strlen(w) >= 1 && strlen(w) <= 39);
        for (const char *p = w; *p; p++)
            ASSERT_TRUE(hints_chord(*p, &a, &s) || (allow_space && *p == ' '));
    }
}

int main(void) {
    words_init(1234);
    ASSERT_EQ(words_count(WB_LETTERS), 25);
    ASSERT_TRUE(words_count(WB_SHORT) >= 30);
    ASSERT_TRUE(words_count(WB_LONG) >= 30);
    ASSERT_TRUE(words_count(WB_PHRASE) >= 8);
    check_bucket(WB_LETTERS, 0);
    check_bucket(WB_SHORT, 0);
    check_bucket(WB_LONG, 0);
    check_bucket(WB_PHRASE, 1);
    /* no immediate repeats */
    const char *prev = words_pick(WB_SHORT);
    for (int k = 0; k < 100; k++) {
        const char *w = words_pick(WB_SHORT);
        ASSERT_TRUE(w != prev);
        prev = w;
    }
    /* word length ranges */
    for (int k = 0; k < 100; k++) {
        ASSERT_EQ((int)strlen(words_pick(WB_LETTERS)), 1);
        size_t ls = strlen(words_pick(WB_SHORT));
        ASSERT_TRUE(ls >= 3 && ls <= 4);
        size_t ll = strlen(words_pick(WB_LONG));
        ASSERT_TRUE(ll >= 5 && ll <= 8);
    }
    TEST_RETURN();
}
