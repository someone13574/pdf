#include "pdf.h"

#include <stdint.h>

#include "test.h"

int32_t add(int32_t a, int32_t b) {
    return a + b;
}

TEST_FUNC(test_add) {
    TEST_ASSERT_EQ_EPS(0.1 + 0.1 + 0.1, 0.3001, 1e-5L);
    TEST_ASSERT_EQ((uint32_t)add(5, 3), (uint32_t)8);
    TEST_ASSERT_EQ(add(-5, 2), -3);

    TEST_ERROR("test %d", 5);
}
