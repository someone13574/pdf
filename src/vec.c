#include "vec.h"

#include <stddef.h>

#include "arena.h"
#include "log.h"

#define MAX_BLOCKS 32

struct Vec {
    size_t len;
    size_t allocated_blocks;
    void** blocks[MAX_BLOCKS];

    Arena* arena;
};

Vec* vec_new(Arena* arena) {
    LOG_INFO_G("vec", "Creating new vector");

    Vec* vec = arena_alloc(arena, sizeof(Vec));
    vec->len = 0;
    vec->allocated_blocks = 0;
    vec->blocks[0] = NULL;
    vec->arena = arena;

    return vec;
}

void vec_push(Vec* vec, void* element) {
    LOG_DEBUG_G("vec", "Pushing element at addr %p", element);

    size_t block_idx =
        8 * sizeof(size_t) - 1 - (size_t)__builtin_clzll(vec->len + 1);
    LOG_ASSERT(block_idx < MAX_BLOCKS, "Vector max length reached");

    if (block_idx >= vec->allocated_blocks) {
        LOG_TRACE_G("vec", "Allocating new vec block at idx %zu", block_idx);

        size_t block_size = (size_t)1 << block_idx;
        vec->blocks[block_idx] =
            arena_alloc(vec->arena, block_size * sizeof(void*));

        vec->allocated_blocks++;
    }

    size_t offset = vec->len - (((size_t)1 << block_idx) - 1);
    LOG_TRACE_G(
        "vec",
        "Element %zu is in block idx %zu at offset %zu",
        vec->len,
        block_idx,
        offset
    );

    vec->blocks[block_idx][offset] = element;
    vec->len++;
}

void* vec_pop(Vec* vec) {
    LOG_DEBUG_G("vec", "Popping element");

    if (vec->len == 0) {
        return NULL;
    }

    void* element = vec_get(vec, vec->len - 1);
    vec->len--;

    return element;
}

void* vec_get(Vec* vec, size_t idx) {
    LOG_DEBUG_G("vec", "Getting element at index %zu", idx);

    if (idx >= vec->len) {
        return NULL;
    }

    size_t block_idx =
        8 * sizeof(size_t) - 1 - (size_t)__builtin_clzll(idx + 1);
    size_t offset = idx - (((size_t)1 << block_idx) - 1);
    LOG_TRACE_G(
        "vec",
        "Element %zu is in block idx %zu at offset %zu",
        idx,
        block_idx,
        offset
    );

    return vec->blocks[block_idx][offset];
}

size_t vec_len(Vec* vec) {
    return vec->len;
}

#ifdef TEST
#include "test.h"

TEST_FUNC(test_vec_new) {
    Arena* arena = arena_new(1024);
    Vec* vec = vec_new(arena);

    TEST_ASSERT(vec);
    TEST_ASSERT_EQ(vec_len(vec), (size_t)0);
    TEST_ASSERT_EQ(vec->allocated_blocks, (size_t)0);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_vec_push_and_get) {
    Arena* arena = arena_new(1024);
    Vec* vec = vec_new(arena);

    int x = 42;
    int y = 89;
    vec_push(vec, &x);
    vec_push(vec, &y);

    TEST_ASSERT_EQ(vec_len(vec), (size_t)2);
    TEST_ASSERT_EQ(vec->allocated_blocks, (size_t)2);

    TEST_ASSERT_EQ(vec_get(vec, 0), (void*)&x);
    TEST_ASSERT_EQ(vec_get(vec, 1), (void*)&y);
    TEST_ASSERT(!vec_get(vec, 3));

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_vec_pop) {
    Arena* arena = arena_new(1024);
    Vec* vec = vec_new(arena);

    int x = 1, y = 2;
    vec_push(vec, &x);
    vec_push(vec, &y);

    void* ptr_y = vec_pop(vec);
    TEST_ASSERT_EQ(ptr_y, (void*)&y);
    TEST_ASSERT_EQ(vec_len(vec), (size_t)1);

    void* ptr_x = vec_pop(vec);
    TEST_ASSERT_EQ(ptr_x, (void*)&x);
    TEST_ASSERT_EQ(vec_len(vec), (size_t)0);

    TEST_ASSERT(!vec_pop(vec));
    TEST_ASSERT_EQ(vec_len(vec), (size_t)0);

    int z = 42;
    vec_push(vec, &z);
    TEST_ASSERT_EQ(vec_len(vec), (size_t)1);

    void* ptr_z = vec_pop(vec);
    TEST_ASSERT_EQ(ptr_z, (void*)&z);
    TEST_ASSERT_EQ(vec_len(vec), (size_t)0);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_vec_growth) {
    Arena* arena = arena_new(1024);
    Vec* vec = vec_new(arena);

    for (int idx = 0; idx < 10; idx++) {
        int* num = arena_alloc(arena, sizeof(int));
        *num = idx;

        vec_push(vec, num);
    }

    TEST_ASSERT_EQ(vec_len(vec), (size_t)10);
    TEST_ASSERT_EQ(vec->allocated_blocks, (size_t)4);

    for (int idx = 0; idx < 10; idx++) {
        int* num = vec_get(vec, (size_t)idx);
        TEST_ASSERT_EQ(*num, idx);
    }

    arena_free(arena);
    return TEST_RESULT_PASS;
}

#endif // TEST
