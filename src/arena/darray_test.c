#ifdef TEST

#include "arena/arena.h"
#include "test.h"

#define DARRAY_NAME MyTestArray
#define DARRAY_LOWERCASE_NAME my_test_array
#define DARRAY_TYPE int
#include "darray_impl.h"

TEST_FUNC(test_array_new) {
    Arena* arena = arena_new(1024);
    MyTestArray* array = my_test_array_new(arena, 3);
    TEST_ASSERT(array);
    TEST_ASSERT_EQ((size_t)3, my_test_array_len(array));

    int out;
    TEST_ASSERT(my_test_array_get(array, 0, &out));
    TEST_ASSERT(my_test_array_get(array, 1, &out));
    TEST_ASSERT(my_test_array_get(array, 2, &out));
    TEST_ASSERT(!my_test_array_get(array, 3, &out));
    TEST_ASSERT(!my_test_array_get(array, 100, &out));

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_array_new_init) {
    Arena* arena = arena_new(1024);
    MyTestArray* array = my_test_array_new_init(arena, 5, 7);
    TEST_ASSERT(array);
    TEST_ASSERT_EQ((size_t)5, my_test_array_len(array));

    int x;
    for (size_t idx = 0; idx < 5; idx++) {
        TEST_ASSERT(my_test_array_get(array, idx, &x));
        TEST_ASSERT_EQ(7, x);
    }
    TEST_ASSERT(!my_test_array_get(array, 5, &x));
    TEST_ASSERT(!my_test_array_get(array, 15, &x));

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_array_new_from) {
    Arena* arena = arena_new(1024);
    int elements[5] = {4, 3, 2, 1, 0};
    MyTestArray* array = my_test_array_new_from(arena, 5, elements);
    TEST_ASSERT(arena);

    int x;
    for (size_t idx = 0; idx < 5; idx++) {
        TEST_ASSERT(my_test_array_get(array, idx, &x));
        TEST_ASSERT_EQ(elements[idx], x);
    }

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_array_set_and_get) {
    Arena* arena = arena_new(1024);
    MyTestArray* array = my_test_array_new_init(arena, 2, 0);
    TEST_ASSERT(array);
    TEST_ASSERT_EQ((size_t)2, my_test_array_len(array));

    my_test_array_set(array, 0, -5);
    my_test_array_set(array, 1, 42);

    int element0, element1;
    TEST_ASSERT(my_test_array_get(array, 0, &element0));
    TEST_ASSERT_EQ(-5, element0);

    TEST_ASSERT(my_test_array_get(array, 1, &element1));
    TEST_ASSERT_EQ(42, element1);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_array_get_ptr) {
    Arena* arena = arena_new(1024);
    MyTestArray* arr = my_test_array_new_init(arena, 3, 3);
    TEST_ASSERT(arr);

    int* ptr0;
    TEST_ASSERT(my_test_array_get_ptr(arr, 0, &ptr0));
    TEST_ASSERT(ptr0);
    TEST_ASSERT_EQ(3, *ptr0);
    *ptr0 = 99;

    int element1;
    TEST_ASSERT(my_test_array_get(arr, 0, &element1));
    TEST_ASSERT_EQ(99, element1);

    int* out_of_bounds_ptr;
    TEST_ASSERT(!my_test_array_get_ptr(arr, 5, &out_of_bounds_ptr));

    arena_free(arena);
    return TEST_RESULT_PASS;
}

#else

void darray_test_translation_unit_not_empty(void) {}

#endif
