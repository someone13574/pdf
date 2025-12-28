#ifdef TEST

#include "arena/arena.h"
#include "test/test.h"

#define DVEC_NAME MyTestVec
#define DVEC_LOWERCASE_NAME my_test_vec
#define DVEC_TYPE int
#include "arena/dvec_impl.h"

TEST_FUNC(test_vec_new) {
    Arena* arena = arena_new(1024);
    MyTestVec* vec = my_test_vec_new(arena);

    TEST_ASSERT(vec);
    TEST_ASSERT_EQ((size_t)0, my_test_vec_len(vec));

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_vec_push_and_get) {
    Arena* arena = arena_new(1024);
    MyTestVec* vec = my_test_vec_new(arena);

    my_test_vec_push(vec, 42);
    my_test_vec_push(vec, 89);

    TEST_ASSERT_EQ(my_test_vec_len(vec), (size_t)2);

    int x;
    int y;
    TEST_ASSERT(my_test_vec_get(vec, 0, &x));
    TEST_ASSERT(my_test_vec_get(vec, 1, &y));
    TEST_ASSERT_EQ(42, x);
    TEST_ASSERT_EQ(89, y);

    int z;
    TEST_ASSERT(!my_test_vec_get(vec, 2, &z));

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_vec_pop) {
    Arena* arena = arena_new(1024);
    MyTestVec* vec = my_test_vec_new(arena);

    my_test_vec_push(vec, 1);
    my_test_vec_push(vec, 2);
    TEST_ASSERT_EQ((size_t)2, my_test_vec_len(vec));

    int x;
    TEST_ASSERT(my_test_vec_pop(vec, &x));
    TEST_ASSERT_EQ(2, x);
    TEST_ASSERT_EQ((size_t)1, my_test_vec_len(vec));

    TEST_ASSERT(my_test_vec_pop(vec, &x));
    TEST_ASSERT_EQ(1, x);
    TEST_ASSERT_EQ((size_t)0, my_test_vec_len(vec));

    TEST_ASSERT(!my_test_vec_pop(vec, &x));
    TEST_ASSERT_EQ((size_t)0, my_test_vec_len(vec));

    my_test_vec_push(vec, 42);
    TEST_ASSERT_EQ((size_t)1, my_test_vec_len(vec));

    TEST_ASSERT(my_test_vec_pop(vec, &x));
    TEST_ASSERT_EQ(42, x);
    TEST_ASSERT_EQ((size_t)0, my_test_vec_len(vec));

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_vec_growth) {
    Arena* arena = arena_new(1024);
    MyTestVec* vec = my_test_vec_new(arena);

    for (int idx = 0; idx < 10; idx++) {
        my_test_vec_push(vec, idx * 2);
    }

    TEST_ASSERT_EQ((size_t)10, my_test_vec_len(vec));

    for (int idx = 0; idx < 10; idx++) {
        int num;
        TEST_ASSERT(my_test_vec_get(vec, (size_t)idx, &num));
        TEST_ASSERT_EQ(idx * 2, num);
    }

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_vec_clone) {
    Arena* arena = arena_new(1024);
    MyTestVec* vec = my_test_vec_new(arena);

    my_test_vec_push(vec, 42);
    my_test_vec_push(vec, 89);

    MyTestVec* cloned = my_test_vec_clone(vec);
    my_test_vec_clear(vec);

    TEST_ASSERT_EQ(my_test_vec_len(vec), (size_t)0);
    TEST_ASSERT_EQ(my_test_vec_len(cloned), (size_t)2);

    my_test_vec_push(vec, 824);

    int x;
    int y;
    TEST_ASSERT(my_test_vec_get(cloned, 0, &x));
    TEST_ASSERT(my_test_vec_get(cloned, 1, &y));
    TEST_ASSERT_EQ(42, x);
    TEST_ASSERT_EQ(89, y);

    int z;
    TEST_ASSERT(!my_test_vec_get(cloned, 2, &z));

    int w;
    TEST_ASSERT(my_test_vec_get(vec, 0, &w));
    TEST_ASSERT_EQ(824, w);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

#else // TEST

void dvec_test_translation_unit_not_empty(void) {}

#endif
