#include "arena.h"

#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "log.h"

static const size_t MAX_BLOCK_SIZE = 1ULL << 30;

typedef struct {
    uintptr_t start;
    uintptr_t end;
    uintptr_t ptr;
} ArenaBlock;

void arena_block_init(ArenaBlock* block, size_t size) {
    LOG_INFO_G("arena", "Allocating new arena block with size %zu", size);

    LOG_ASSERT(
        (size != 0) && (size & (size - 1)) == 0,
        "Invalid arena block size %zu. Must be a power of two",
        size
    );
    LOG_ASSERT(size <= MAX_BLOCK_SIZE, "Arena block size %zu too large", size);

    void* alloc = malloc(size);
    if (!alloc) {
        LOG_ERROR_G("arena", "Arena block of size %zu allocation failed", size);
        exit(EXIT_FAILURE);
    }

    block->start = (uintptr_t)alloc;
    block->end = block->start + size;
    block->ptr = block->end;
}

size_t align_ptr_down(uintptr_t ptr, size_t align) {
    return ptr & ~(align - 1);
}

struct Arena {
    ArenaBlock* blocks;
    size_t num_blocks;

    size_t next_block_size;
};

Arena* arena_new(size_t block_size) {
    ArenaBlock* blocks = malloc(sizeof(ArenaBlock));
    LOG_ASSERT(blocks, "Malloc failed");
    arena_block_init(&blocks[0], block_size);

    Arena* arena = malloc(sizeof(Arena));
    LOG_ASSERT(arena, "Malloc failed");
    arena->blocks = blocks;
    arena->num_blocks = 1;
    arena->next_block_size = block_size;

    return arena;
}

void arena_free(Arena* arena) {
    if (!arena) {
        return;
    }

    for (size_t block = 0; block < arena->num_blocks; block++) {
        free((void*)arena->blocks[block].start);
    }
    free(arena->blocks);

    arena->blocks = NULL;
    arena->num_blocks = 0;
    arena->next_block_size = 0;
}

void* arena_alloc(Arena* arena, size_t size) {
    return arena_alloc_align(arena, size, alignof(max_align_t));
}

void* arena_alloc_align(Arena* arena, size_t size, size_t align) {
    DBG_ASSERT(arena);
    DBG_ASSERT(arena->blocks);
    DBG_ASSERT(size > 0);
    DBG_ASSERT(align > 0);
    DBG_ASSERT((align & (align - 1)) == 0);

    LOG_DEBUG_G(
        "arena",
        "Allocating %zu bytes on arena with align %zu",
        size,
        align
    );

    // Find existing block
    for (size_t block_idx = 0; block_idx < arena->num_blocks; block_idx++) {
        ArenaBlock* block = &arena->blocks[block_idx];

        // Check if this block can fit the allocation
        uintptr_t aligned_ptr = align_ptr_down(block->ptr - size, align);
        if (aligned_ptr < block->start) {
            // Try next block
            continue;
        }

        // Use this block
        block->ptr = aligned_ptr;

        LOG_TRACE_G(
            "arena",
            "Allocating on block %zu. Usage: %zu/%zu bytes remaining",
            block_idx,
            block->ptr - block->start,
            block->end - block->start
        );

        return (void*)aligned_ptr;
    }

    // Create new block
    ArenaBlock* new_array =
        realloc(arena->blocks, sizeof(ArenaBlock) * (arena->num_blocks + 1));
    if (!new_array) {
        LOG_ERROR_G("arena", "Arena block list allocation failed");
        exit(EXIT_FAILURE);
    }

    arena->blocks = new_array;
    arena->num_blocks++;

    while (arena->next_block_size
           < size + (align == alignof(max_align_t) ? 0 : align)
    ) { // Ensure there is enough space
        LOG_ASSERT(
            arena->next_block_size < MAX_BLOCK_SIZE / 2,
            "Arena allocations cannot be larger than 1 GiB"
        );
        arena->next_block_size <<= 1;
    }

    ArenaBlock* block = &arena->blocks[arena->num_blocks - 1];
    arena_block_init(
        &arena->blocks[arena->num_blocks - 1],
        arena->next_block_size
    );
    if (arena->next_block_size <= MAX_BLOCK_SIZE / 2) {
        arena->next_block_size <<= 1;
    }

    // Allocate space in block
    uintptr_t aligned_ptr = align_ptr_down(block->ptr - size, align);
    LOG_ASSERT(
        aligned_ptr >= block->start,
        "Unreachable: not enough space in newly allocated block"
    );
    block->ptr = aligned_ptr;

    return (void*)aligned_ptr;
}

void arena_reset(Arena* arena) {
    DBG_ASSERT(arena);
    DBG_ASSERT(arena->blocks);

    LOG_DEBUG_G("arena", "Resetting arena");

    for (size_t block_idx = 0; block_idx < arena->num_blocks; block_idx++) {
        ArenaBlock* block = &arena->blocks[block_idx];
        block->ptr = block->end;
    }
}

#ifdef TEST
#include "test.h"

TEST_FUNC(test_arena_simple_alloc) {
    Arena* arena = arena_new(1024);
    void* ptr_a = arena_alloc(arena, 16);
    void* ptr_b = arena_alloc(arena, 32);

    TEST_ASSERT(ptr_a);
    TEST_ASSERT(ptr_b);
    TEST_ASSERT_NE(ptr_a, ptr_b);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_arena_alignment) {
    Arena* arena = arena_new(1024);

    void* ptr = arena_alloc_align(arena, 15, 64);
    TEST_ASSERT(ptr);

    uintptr_t addr = (uintptr_t)ptr;
    TEST_ASSERT_EQ((uintptr_t)0, addr % 64);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_arena_large_alloc) {
    Arena* arena = arena_new(64);
    void* ptr = arena_alloc(arena, 1000);

    TEST_ASSERT(ptr);
    TEST_ASSERT_EQ(arena->num_blocks, (size_t)2);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_arena_reset) {
    Arena* arena = arena_new(128);

    void* ptr_a = arena_alloc(arena, 20);
    TEST_ASSERT(ptr_a);

    arena_reset(arena);

    void* ptr_b = arena_alloc(arena, 20);
    TEST_ASSERT(ptr_b);

    TEST_ASSERT_EQ(ptr_a, ptr_b);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_arena_fill) {
    Arena* arena = arena_new(256);

    // Fills a single block
    void* ptrs[4];
    for (int idx = 0; idx < 4; idx++) {
        ptrs[idx] = arena_alloc(arena, 64);
        TEST_ASSERT(ptrs[idx]);
    }

    TEST_ASSERT_EQ(arena->num_blocks, (size_t)1);

    // Creates a new block
    void* extra_ptr = arena_alloc(arena, 8);
    TEST_ASSERT(extra_ptr);
    TEST_ASSERT_EQ(arena->num_blocks, (size_t)2);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

#endif // TEST
