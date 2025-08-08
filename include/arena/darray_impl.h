#ifndef DARRAY_IMPL_H
#define DARRAY_IMPL_H

#include <string.h>

#include "arena/arena.h"
#include "logger/log.h"

#endif // DARRAY_IMPL_H

// Check arguments
#ifndef DARRAY_NAME
#error "DARRAY_NAME is not defined"
#define DARRAY_NAME PlaceholderArray
#endif // DARRAY_NAME

#ifndef DARRAY_LOWERCASE_NAME
#error "DARRAY_LOWERCASE_NAME is not defined"
#define DARRAY_LOWERCASE_NAME placeholder_array
#endif // DARRAY_LOWERCASE_NAME

#ifndef DARRAY_TYPE
#error "DARRAY_TYPE is not defined"
#define DARRAY_TYPE int
#endif // DARRAY_TYPE

// Helper macros
#define _CAT(a, b) a##b
#define CAT(a, b) _CAT(a, b)
#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)
#define DARRAY_FN(name) CAT(DARRAY_LOWERCASE_NAME, CAT(_, name))

// An arena-backed fixed-size array
typedef struct DARRAY_NAME DARRAY_NAME;

struct DARRAY_NAME {
    size_t len;
    DARRAY_TYPE* elements;
};

// Creates a new arena-backed array of a set size with uninitialized elements
DARRAY_NAME* DARRAY_FN(new)(Arena* arena, size_t num_elements) {
    RELEASE_ASSERT(arena);

    LOG_DIAG(
        DEBUG,
        ARRAY,
        "Creating new " STRINGIFY(DARRAY_NAME) " (Array<" STRINGIFY(DARRAY_TYPE
        ) ", %zu>)",
        num_elements
    );

    DARRAY_NAME* array = arena_alloc(arena, sizeof(DARRAY_NAME));
    array->len = num_elements;

    if (num_elements != 0) {
        array->elements =
            arena_alloc(arena, sizeof(DARRAY_TYPE) * num_elements);
    } else {
        array->elements = NULL;
    }

    return array;
}

// Creates a new arena-backed array of a set size with initialized elements
DARRAY_NAME* DARRAY_FN(new_init)(
    Arena* arena,
    size_t num_elements,
    DARRAY_TYPE initial_value
) {
    RELEASE_ASSERT(arena);

    DARRAY_NAME* array = DARRAY_FN(new)(arena, num_elements);
    for (size_t idx = 0; idx < num_elements; idx++) {
        array->elements[idx] = initial_value;
    }

    return array;
}

// Creates a new arena-backed array from an array initializer list
DARRAY_NAME* DARRAY_FN(new_from)(
    Arena* arena,
    size_t num_elements,
    DARRAY_TYPE arr[num_elements]
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(arr);

    DARRAY_NAME* array = DARRAY_FN(new)(arena, num_elements);
    memcpy(array->elements, arr, sizeof(DARRAY_TYPE) * num_elements);

    return array;
}

// Gets the element stored at index `idx` and stores it in `out`. If the index
// is out of bounds, false is returned and `out` isn't set.
bool DARRAY_FN(get)(DARRAY_NAME* array, size_t idx, DARRAY_TYPE* out) {
    RELEASE_ASSERT(array);
    RELEASE_ASSERT(out);

    LOG_DIAG(
        DEBUG,
        ARRAY,
        "Getting " STRINGIFY(DARRAY_TYPE
        ) " element at idx %zu from array " STRINGIFY(DARRAY_NAME),
        idx
    );

    if (idx >= array->len) {
        return false;
    }

    *out = array->elements[idx];
    return true;
}

// Gets the element stored at index `idx` and stores a pointer to it in `out`.
// If the index is out of bounds, false is returned and `out` isn't set.
bool DARRAY_FN(get_ptr)(DARRAY_NAME* array, size_t idx, DARRAY_TYPE** out) {
    RELEASE_ASSERT(array);
    RELEASE_ASSERT(out);

    LOG_DIAG(
        DEBUG,
        ARRAY,
        "Getting reference to " STRINGIFY(DARRAY_TYPE
        ) " element at idx %zu from array " STRINGIFY(DARRAY_NAME),
        idx
    );

    if (idx >= array->len) {
        return false;
    }

    *out = &array->elements[idx];
    return true;
}

// Sets the element stored at index `idx`, which must be in bounds.
void DARRAY_FN(set)(DARRAY_NAME* array, size_t idx, DARRAY_TYPE value) {
    RELEASE_ASSERT(array);
    RELEASE_ASSERT(idx < array->len);

    array->elements[idx] = value;
}

// Gets the length of the array
size_t DARRAY_FN(len)(DARRAY_NAME* array) {
    RELEASE_ASSERT(array);

    return array->len;
}

// Undefines
#undef DARRAY_NAME
#undef DARRAY_LOWERCASE_NAME
#undef DARRAY_TYPE
#undef _CAT
#undef CAT
#undef _STRINGIFY
#undef STRINGIFY
#undef DARRAY_FN
