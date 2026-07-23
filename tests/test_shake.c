#include "test_util.h"
#include "shake.h"

int main(void) {
    shake_t s;
    shake_init(&s);
    /* at rest (1 g down): never fires */
    for (uint32_t t = 0; t < 2000; t += 20)
        ASSERT_TRUE(!shake_feed(&s, 0.0f, 0.0f, 1.0f, t));
    /* hard shake fires once, then locks out */
    ASSERT_TRUE(shake_feed(&s, 2.0f, 2.0f, 0.5f, 2000));
    ASSERT_TRUE(!shake_feed(&s, 2.0f, 2.0f, 0.5f, 2020));
    ASSERT_TRUE(!shake_feed(&s, 3.0f, 0.0f, 0.0f, 2600));
    /* after the 800 ms lockout it can fire again */
    ASSERT_TRUE(shake_feed(&s, 3.0f, 0.0f, 0.0f, 2801));
    /* moderate motion (1.5 g) is below threshold */
    shake_init(&s);
    ASSERT_TRUE(!shake_feed(&s, 1.5f, 0.0f, 0.0f, 100));
    TEST_RETURN();
}
