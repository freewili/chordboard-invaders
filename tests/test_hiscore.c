#include "test_util.h"
#include "hiscore.h"
#include <string.h>

int main(void) {
    hs_table_t t;
    hs_clear(&t);
    ASSERT_EQ(t.count, 0); ASSERT_EQ(t.best_wpm, 0);

    /* empty table: every score qualifies at rank 0 */
    ASSERT_EQ(hs_rank(&t, 10), 0);

    hs_insert(&t, "AAA", 100, 12);
    hs_insert(&t, "BBB", 300, 20);
    hs_insert(&t, "CCC", 200, 15);
    ASSERT_EQ(t.count, 3);
    ASSERT_TRUE(strcmp(t.e[0].initials, "BBB") == 0);   /* sorted desc */
    ASSERT_TRUE(strcmp(t.e[1].initials, "CCC") == 0);
    ASSERT_TRUE(strcmp(t.e[2].initials, "AAA") == 0);
    ASSERT_EQ(hs_rank(&t, 250), 1);
    ASSERT_EQ(hs_rank(&t, 50), 3);

    /* fill to 10; an 11th low score no longer qualifies */
    for (int i = 0; i < 7; i++) hs_insert(&t, "DDD", 400 + i, 10);
    ASSERT_EQ(t.count, 10);
    ASSERT_EQ(hs_rank(&t, 1), -1);
    ASSERT_TRUE(hs_rank(&t, 500) >= 0);
    uint32_t lowest_before = t.e[9].score;
    hs_insert(&t, "EEE", 999, 30);
    ASSERT_EQ(t.count, 10);                              /* stayed capped */
    ASSERT_TRUE(t.e[9].score >= lowest_before);          /* lowest dropped */
    ASSERT_TRUE(strcmp(t.e[0].initials, "EEE") == 0);

    hs_note_wpm(&t, 25); ASSERT_EQ(t.best_wpm, 25);
    hs_note_wpm(&t, 19); ASSERT_EQ(t.best_wpm, 25);      /* keeps max */

    /* round-trip */
    uint8_t blob[HS_BLOB_SIZE];
    hs_encode(&t, blob);
    hs_table_t u;
    ASSERT_TRUE(hs_decode(&u, blob));
    ASSERT_EQ(u.count, t.count);
    ASSERT_EQ(u.best_wpm, t.best_wpm);
    ASSERT_TRUE(memcmp(u.e, t.e, sizeof u.e) == 0);

    /* corruption is rejected */
    blob[10] ^= 0xFF;
    ASSERT_TRUE(!hs_decode(&u, blob));
    /* erased-flash blob (all 0xFF) is rejected */
    memset(blob, 0xFF, sizeof blob);
    ASSERT_TRUE(!hs_decode(&u, blob));
    TEST_RETURN();
}
