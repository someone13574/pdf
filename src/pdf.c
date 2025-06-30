#include "pdf.h"

#include <stdint.h>

#include "log.h"
#include "test.h"

int32_t add(int32_t a, int32_t b) {
    return a + b;
}

TEST_FUNC(test_add) {
    // TEST_ASSERT_EQ_EPS(0.1 + 0.1 + 0.1, 0.3001, 1e-5L);
    TEST_ASSERT_EQ((uint32_t)add(5, 3), (uint32_t)8);
    TEST_ASSERT_EQ(add(-5, 2), -3);

    return TEST_SUCCESS;
}

TEST_FUNC(test2) {
    LOG_DEBUG("Hello");
    LOG_WARN("Something is going wrong");
    TEST_ASSERT(5 < 3, "err");

    return TEST_SUCCESS;
}

TEST_FUNC(test3) {
    LOG_DEBUG("Hi!");
    return TEST_SUCCESS;
}
