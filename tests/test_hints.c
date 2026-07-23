#include "test_util.h"
#include "hints.h"

int main(void) {
    int a, b;
    ASSERT_TRUE(hints_chord('a', &a, &b)); ASSERT_EQ(a, 0); ASSERT_EQ(b, 0);
    ASSERT_TRUE(hints_chord('e', &a, &b)); ASSERT_EQ(a, 0); ASSERT_EQ(b, 4);
    ASSERT_TRUE(hints_chord('f', &a, &b)); ASSERT_EQ(a, 1); ASSERT_EQ(b, 0);
    ASSERT_TRUE(hints_chord('m', &a, &b)); ASSERT_EQ(a, 2); ASSERT_EQ(b, 2);
    ASSERT_TRUE(hints_chord('t', &a, &b)); ASSERT_EQ(a, 3); ASSERT_EQ(b, 4);
    ASSERT_TRUE(hints_chord('y', &a, &b)); ASSERT_EQ(a, 4); ASSERT_EQ(b, 4);
    ASSERT_TRUE(!hints_chord('z', &a, &b));   /* z is not on the LOWER page */
    ASSERT_TRUE(!hints_chord('A', &a, &b));
    ASSERT_TRUE(!hints_chord(' ', &a, &b));
    ASSERT_TRUE(!hints_chord(0, &a, &b));
    TEST_RETURN();
}
