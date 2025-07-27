#ifdef TEST

#include "test/test.h"

#define DLINKED_NAME MyTestList
#define DLINKED_LOWERCASE_NAME my_test_list
#define DLINKED_TYPE int
#include "arena/dlinked_impl.h"

TEST_FUNC(test_list_new) {
    Arena* arena = arena_new(1024);
    MyTestList* list = my_test_list_new(arena);

    TEST_ASSERT(list);
    TEST_ASSERT_EQ((size_t)0, my_test_list_len(list));

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_push_back_and_get) {
    Arena* arena = arena_new(1024);
    MyTestList* list = my_test_list_new(arena);

    my_test_list_push_back(list, 10);
    my_test_list_push_back(list, 20);
    my_test_list_push_back(list, 30);

    TEST_ASSERT_EQ((size_t)3, my_test_list_len(list));

    int x, y, z;
    TEST_ASSERT(my_test_list_get(list, 0, &x));
    TEST_ASSERT(my_test_list_get(list, 1, &y));
    TEST_ASSERT(my_test_list_get(list, 2, &z));
    TEST_ASSERT_EQ(10, x);
    TEST_ASSERT_EQ(20, y);
    TEST_ASSERT_EQ(30, z);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_push_front_and_order) {
    Arena* arena = arena_new(1024);
    MyTestList* list = my_test_list_new(arena);

    my_test_list_push_front(list, 5);
    my_test_list_push_front(list, 15);

    TEST_ASSERT_EQ((size_t)2, my_test_list_len(list));

    int a, b;
    TEST_ASSERT(my_test_list_get(list, 0, &b));
    TEST_ASSERT(my_test_list_get(list, 1, &a));
    TEST_ASSERT_EQ(15, b);
    TEST_ASSERT_EQ(5, a);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_pop_front_and_back) {
    Arena* arena = arena_new(1024);
    MyTestList* list = my_test_list_new(arena);

    my_test_list_push_back(list, 1);
    my_test_list_push_back(list, 2);
    my_test_list_push_back(list, 3);

    int element;
    TEST_ASSERT(my_test_list_pop_front(list, &element));
    TEST_ASSERT_EQ(1, element);
    TEST_ASSERT_EQ((size_t)2, my_test_list_len(list));

    TEST_ASSERT(my_test_list_pop_back(list, &element));
    TEST_ASSERT_EQ(3, element);
    TEST_ASSERT_EQ((size_t)1, my_test_list_len(list));

    TEST_ASSERT(my_test_list_pop_front(list, &element));
    TEST_ASSERT_EQ(2, element);
    TEST_ASSERT_EQ((size_t)0, my_test_list_len(list));

    // Popping empty
    TEST_ASSERT(!my_test_list_pop_back(list, &element));
    TEST_ASSERT(!my_test_list_pop_front(list, &element));

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_insert_and_remove) {
    Arena* arena = arena_new(1024);
    MyTestList* list = my_test_list_new(arena);

    my_test_list_push_back(list, 0);
    my_test_list_push_back(list, 2);
    my_test_list_push_back(list, 3);

    my_test_list_insert(list, 1, 1);
    TEST_ASSERT_EQ((size_t)4, my_test_list_len(list));

    int val;
    for (size_t idx = 0; idx < 4; idx++) {
        TEST_ASSERT(my_test_list_get(list, idx, &val));
        TEST_ASSERT_EQ((int)idx, val);
    }

    int removed = my_test_list_remove(list, 2);
    TEST_ASSERT_EQ(2, removed);
    TEST_ASSERT_EQ((size_t)3, my_test_list_len(list));

    TEST_ASSERT(my_test_list_get(list, 2, &val));
    TEST_ASSERT_EQ(3, val);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_set_and_get_ptr) {
    Arena* arena = arena_new(1024);
    MyTestList* list = my_test_list_new(arena);

    for (int idx = 0; idx < 5; idx++) {
        my_test_list_push_back(list, idx);
    }

    my_test_list_set(list, 2, 42);

    int v;
    TEST_ASSERT(my_test_list_get(list, 2, &v));
    TEST_ASSERT_EQ(42, v);

    // get_ptr
    int* ptr;
    TEST_ASSERT(my_test_list_get_ptr(list, 2, &ptr));
    TEST_ASSERT(ptr);
    TEST_ASSERT_EQ(42, *ptr);

    // modify via ptr
    *ptr = 100;
    TEST_ASSERT(my_test_list_get(list, 2, &v));
    TEST_ASSERT_EQ(100, v);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_get_out_of_bounds) {
    Arena* arena = arena_new(1024);
    MyTestList* list = my_test_list_new(arena);
    int dummy;
    int* dummy_ptr;

    // empty list
    TEST_ASSERT(!my_test_list_get(list, 0, &dummy));
    TEST_ASSERT(!my_test_list_get_ptr(list, 5, &dummy_ptr));

    // single element
    my_test_list_push_back(list, 7);
    TEST_ASSERT(!my_test_list_get(list, 1, &dummy));

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_mixed_operations) {
    Arena* arena = arena_new(1024);
    MyTestList* list = my_test_list_new(arena);
    int v;

    // mix: push_back, push_front, insert
    my_test_list_push_back(list, 1); // [1]
    my_test_list_push_front(list, 0); // [0,1]
    my_test_list_insert(list, 2, 3); // [0,1,3]
    my_test_list_insert(list, 2, 2); // [0,1,2,3]
    TEST_ASSERT_EQ((size_t)4, my_test_list_len(list));

    for (size_t idx = 0; idx < 4; idx++) {
        TEST_ASSERT(my_test_list_get(list, idx, &v));
        TEST_ASSERT_EQ((int)idx, v);
    }

    // mix pops
    TEST_ASSERT(my_test_list_pop_front(list, &v)); // 0
    TEST_ASSERT_EQ(0, v);
    TEST_ASSERT(my_test_list_pop_back(list, &v)); // 3
    TEST_ASSERT_EQ(3, v);
    TEST_ASSERT_EQ((size_t)2, my_test_list_len(list));

    TEST_ASSERT(my_test_list_get(list, 0, &v)); // 1
    TEST_ASSERT_EQ(1, v);
    TEST_ASSERT(my_test_list_get(list, 1, &v)); // 2
    TEST_ASSERT_EQ(2, v);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_clear) {
    Arena* arena = arena_new(1024);
    MyTestList* list = my_test_list_new(arena);

    // initial operations
    my_test_list_push_back(list, 1);
    my_test_list_push_back(list, 2);
    TEST_ASSERT_EQ((size_t)2, my_test_list_len(list));

    // clear and verify empty state
    my_test_list_clear(list);
    TEST_ASSERT_EQ((size_t)0, my_test_list_len(list));
    int v;
    TEST_ASSERT(!my_test_list_pop_back(list, &v));
    TEST_ASSERT(!my_test_list_pop_front(list, &v));
    TEST_ASSERT(!my_test_list_get(list, 0, &v));

    // post-clear operations should work correctly
    my_test_list_push_back(list, 5);
    my_test_list_push_front(list, 3);
    TEST_ASSERT_EQ((size_t)2, my_test_list_len(list));
    TEST_ASSERT(my_test_list_get(list, 0, &v));
    TEST_ASSERT_EQ(3, v);
    TEST_ASSERT(my_test_list_get(list, 1, &v));
    TEST_ASSERT_EQ(5, v);

    // popping post-clear
    TEST_ASSERT(my_test_list_pop_front(list, &v));
    TEST_ASSERT_EQ(3, v);
    TEST_ASSERT(my_test_list_pop_back(list, &v));
    TEST_ASSERT_EQ(5, v);
    TEST_ASSERT_EQ((size_t)0, my_test_list_len(list));

    arena_free(arena);
    return TEST_RESULT_PASS;
}

static bool int_less(int* lhs, int* rhs) {
    return *lhs < *rhs;
}

TEST_FUNC(test_insert_sorted_ascending) {
    Arena* arena = arena_new(1024);
    MyTestList* list = my_test_list_new(arena);
    int element;

    // Start with sorted [1,3,5]
    my_test_list_push_back(list, 1);
    my_test_list_push_back(list, 3);
    my_test_list_push_back(list, 5);

    // Insert at front
    my_test_list_insert_sorted(list, 0, int_less, true);
    TEST_ASSERT_EQ((size_t)4, my_test_list_len(list));
    TEST_ASSERT(my_test_list_get(list, 0, &element));
    TEST_ASSERT_EQ(0, element);

    // Insert in middle
    my_test_list_insert_sorted(list, 4, int_less, true);
    TEST_ASSERT_EQ((size_t)5, my_test_list_len(list));
    TEST_ASSERT(my_test_list_get(list, 3, &element));
    TEST_ASSERT_EQ(4, element);

    // Insert at end
    my_test_list_insert_sorted(list, 6, int_less, true);
    TEST_ASSERT_EQ((size_t)6, my_test_list_len(list));
    TEST_ASSERT(my_test_list_get(list, 5, &element));
    TEST_ASSERT_EQ(6, element);

    // Verify full order
    int expected[] = {0, 1, 3, 4, 5, 6};
    for (size_t idx = 0; idx < 6; idx++) {
        TEST_ASSERT(my_test_list_get(list, idx, &element));
        TEST_ASSERT_EQ(expected[idx], element);
    }

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_insert_sorted_descending) {
    Arena* arena = arena_new(1024);
    MyTestList* list = my_test_list_new(arena);
    int element;

    // Start with sorted descending [5,3,1]
    my_test_list_push_back(list, 5);
    my_test_list_push_back(list, 3);
    my_test_list_push_back(list, 1);

    // Insert at front (largest)
    my_test_list_insert_sorted(list, 6, int_less, false);
    TEST_ASSERT_EQ((size_t)4, my_test_list_len(list));
    TEST_ASSERT(my_test_list_get(list, 0, &element));
    TEST_ASSERT_EQ(6, element);

    // Insert in middle
    my_test_list_insert_sorted(list, 4, int_less, false);
    TEST_ASSERT_EQ((size_t)5, my_test_list_len(list));
    TEST_ASSERT(my_test_list_get(list, 2, &element));
    TEST_ASSERT_EQ(4, element);

    // Insert at end (smallest)
    my_test_list_insert_sorted(list, 0, int_less, false);
    TEST_ASSERT_EQ((size_t)6, my_test_list_len(list));
    TEST_ASSERT(my_test_list_get(list, 5, &element));
    TEST_ASSERT_EQ(0, element);

    // Verify full order
    int expected[] = {6, 5, 4, 3, 1, 0};
    for (size_t idx = 0; idx < 6; idx++) {
        TEST_ASSERT(my_test_list_get(list, idx, &element));
        TEST_ASSERT_EQ(expected[idx], element);
    }

    arena_free(arena);
    return TEST_RESULT_PASS;
}

#else

void dlinked_test_translation_unit_not_empty(void) {}

#endif
