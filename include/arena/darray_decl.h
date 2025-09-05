#ifndef DARRAY_DECL_H
#define DARRAY_DECL_H

#include <stdbool.h>

#include "arena.h"

#endif // DARRAY_DECL_H

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
#define DARRAY_FN(name) CAT(DARRAY_LOWERCASE_NAME, CAT(_, name))

// An arena-backed fixed-size array
typedef struct DARRAY_NAME DARRAY_NAME;

// Creates a new arena-backed array of a set size with uninitialized elements
DARRAY_NAME* DARRAY_FN(new)(Arena* arena, size_t num_elements);

// Creates a new arena-backed array of a set size with initialized elements
DARRAY_NAME* DARRAY_FN(new_init)(
    Arena* arena,
    size_t num_elements,
    DARRAY_TYPE initial_value
);

// Creates a new arena-backed array from an array initializer list
DARRAY_NAME*
    DARRAY_FN(new_from)(Arena* arena, size_t num_elements, DARRAY_TYPE* arr);

// Gets the element stored at index `idx` and stores it in `out`. If the index
// is out of bounds, false is returned and `out` isn't set.
bool DARRAY_FN(get)(DARRAY_NAME* array, size_t idx, DARRAY_TYPE* out);

// Gets the element stored at index `idx` and stores a pointer to it in `out`.
// If the index is out of bounds, false is returned and `out` isn't set.
bool DARRAY_FN(get_ptr)(DARRAY_NAME* array, size_t idx, DARRAY_TYPE** out);

// Sets the element stored at index `idx`, which must be in bounds.
void DARRAY_FN(set)(DARRAY_NAME* array, size_t idx, DARRAY_TYPE value);

// Gets the length of the array.
size_t DARRAY_FN(len)(DARRAY_NAME* array);

// Gets the raw array and its length.
DARRAY_TYPE* DARRAY_FN(get_raw)(DARRAY_NAME* array, size_t* len_out);

// Undefines
#undef DARRAY_NAME
#undef DARRAY_LOWERCASE_NAME
#undef DARRAY_TYPE
#undef _CAT
#undef CAT
#undef DARRAY_FN
