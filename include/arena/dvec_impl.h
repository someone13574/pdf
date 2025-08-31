#ifndef DVEC_IMPL_H
#define DVEC_IMPL_H

#include <stdbool.h>

#include "arena/arena.h"
#include "logger/log.h"

#ifdef _MSC_VER
#include <intrin.h>
#pragma intrinsic(_BitScanReverse64)
static int clzll(unsigned long long x) {
    unsigned long index;
    unsigned char is_nonzero = _BitScanReverse64(&index, x);

    if (is_nonzero) {
        return 63 - index;
    }

    return 64;
}
#else
static int clzll(unsigned long long x) {
    return __builtin_clzll(x);
}
#endif

#endif // DVEC_IMPL_H

// Check arguments
#ifndef DVEC_NAME
#error "DVEC_NAME is not defined"
#define DVEC_NAME PlaceholderVec
#endif // DVEC_NAME

#ifndef DVEC_LOWERCASE_NAME
#error "DVEC_LOWERCASE_NAME is not defined"
#define DVEC_LOWERCASE_NAME placeholder_vec
#endif // DVEC_LOWERCASE_NAME

#ifndef DVEC_TYPE
#error "DVEC_TYPE is not defined"
#define DVEC_TYPE int
#endif // DVEC_TYPE

// Helper macros
#define _CAT(a, b) a##b
#define CAT(a, b) _CAT(a, b)
#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)
#define DVEC_FN(name) CAT(DVEC_LOWERCASE_NAME, CAT(_, name))

#define DVEC_MAX_BLOCKS 32

// An arena-backed growable vector
typedef struct DVEC_NAME DVEC_NAME;

struct DVEC_NAME {
    Arena* arena;

    size_t len;
    size_t allocated_blocks;
    DVEC_TYPE* blocks[DVEC_MAX_BLOCKS];
};

// Creates a new arena-backed vector with zero elements
DVEC_NAME* DVEC_FN(new)(Arena* arena) {
    RELEASE_ASSERT(arena);

    LOG_DIAG(
        DEBUG,
        VEC,
        "Creating new " STRINGIFY(DVEC_NAME) " (Vec<" STRINGIFY(DVEC_TYPE) ">)"
    );

    DVEC_NAME* vec = arena_alloc(arena, sizeof(DVEC_NAME));
    vec->arena = arena;
    vec->len = 0;
    vec->allocated_blocks = 0;
    vec->blocks[0] = NULL;

    return vec;
}

// Pushes an element to the vector
DVEC_TYPE* DVEC_FN(push)(DVEC_NAME* vec, DVEC_TYPE element) {
    RELEASE_ASSERT(vec);

    LOG_DIAG(
        DEBUG,
        VEC,
        "Pushing " STRINGIFY(DVEC_TYPE) " to " STRINGIFY(DVEC_NAME) " vec"
    );

    size_t block_idx =
        8 * sizeof(size_t) - 1 - (size_t)clzll(vec->len + 1);
    RELEASE_ASSERT(block_idx < DVEC_MAX_BLOCKS, "Vector max length reached");

    if (block_idx >= vec->allocated_blocks) {
        LOG_DIAG(TRACE, VEC, "Allocating new vec block at idx %zu", block_idx);

        size_t block_size = (size_t)1 << block_idx;
        vec->blocks[block_idx] =
            arena_alloc(vec->arena, block_size * sizeof(DVEC_TYPE));

        vec->allocated_blocks++;
    }

    size_t offset = vec->len - (((size_t)1 << block_idx) - 1);
    LOG_DIAG(
        TRACE,
        VEC,
        "Element %zu is in block idx %zu at offset %zu",
        vec->len,
        block_idx,
        offset
    );

    vec->blocks[block_idx][offset] = element;
    vec->len++;

    return &vec->blocks[block_idx][offset];
}

// Gets the element stored at index `idx`, storing the element in `out` and
// returning a bool indicating whether the operation was successful.
bool DVEC_FN(get)(DVEC_NAME* vec, size_t idx, DVEC_TYPE* out) {
    RELEASE_ASSERT(vec);
    RELEASE_ASSERT(out);

    LOG_DIAG(
        DEBUG,
        VEC,
        "Getting " STRINGIFY(DVEC_TYPE
        ) " element at idx %zu from vec " STRINGIFY(DVEC_NAME),
        idx
    );

    if (idx >= vec->len) {
        return false;
    }

    size_t block_idx =
        8 * sizeof(size_t) - 1 - (size_t)clzll(idx + 1);
    size_t offset = idx - (((size_t)1 << block_idx) - 1);
    LOG_DIAG(
        TRACE,
        VEC,
        "Element %zu is in block idx %zu at offset %zu",
        idx,
        block_idx,
        offset
    );

    *out = vec->blocks[block_idx][offset];
    return true;
}

// Gets the element stored at index `idx`, storing a pointer to the element in
// `out` and returning a bool indicating whether the operation was successful.
bool DVEC_FN(get_ptr)(DVEC_NAME* vec, size_t idx, DVEC_TYPE** out) {
    RELEASE_ASSERT(vec);
    RELEASE_ASSERT(out);

    LOG_DIAG(
        DEBUG,
        VEC,
        "Getting " STRINGIFY(DVEC_TYPE
        ) " element ptr at idx %zu from vec " STRINGIFY(DVEC_NAME),
        idx
    );

    if (idx >= vec->len) {
        return false;
    }

    size_t block_idx =
        8 * sizeof(size_t) - 1 - (size_t)clzll(idx + 1);
    size_t offset = idx - (((size_t)1 << block_idx) - 1);
    LOG_DIAG(
        TRACE,
        VEC,
        "Element %zu is in block idx %zu at offset %zu",
        idx,
        block_idx,
        offset
    );

    *out = &vec->blocks[block_idx][offset];
    return true;
}

// Pops an element from the vector, storing the pop'ed element in `out` and
// returning a bool indicating whether the operation was successful.
bool DVEC_FN(pop)(DVEC_NAME* vec, DVEC_TYPE* out) {
    RELEASE_ASSERT(vec);
    RELEASE_ASSERT(out);

    LOG_DIAG(
        DEBUG,
        VEC,
        "Popping " STRINGIFY(DVEC_TYPE) "  element from " STRINGIFY(DVEC_NAME)
    );

    if (vec->len == 0) {
        return false;
    }

    if (!DVEC_FN(get)(vec, vec->len - 1, out)) {
        return false;
    }

    vec->len--;
    return true;
}

// Clears the vector, making it's length zero. This does not free any memory.
void DVEC_FN(clear)(DVEC_NAME* vec) {
    RELEASE_ASSERT(vec);
    vec->len = 0;
}

// Returns the length of the vector.
size_t DVEC_FN(len)(DVEC_NAME* vec) {
    RELEASE_ASSERT(vec);
    return vec->len;
}

// Undefines
#undef DVEC_NAME
#undef DVEC_LOWERCASE_NAME
#undef DVEC_TYPE
#undef _CAT
#undef CAT
#undef _STRINGIFY
#undef STRINGIFY
#undef DVEC_FN
#undef DVEC_MAX_BLOCKS
