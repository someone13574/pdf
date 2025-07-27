#ifndef DVEC_DECL_H
#define DVEC_DECL_H

#include "arena/arena.h"

#endif // DVEC_DECL_H

// Check arguments
#ifndef DVEC_NAME
#warning "DVEC_NAME is not defined"
#define DVEC_NAME PlaceholderVec
#endif // DVEC_NAME

#ifndef DVEC_LOWERCASE_NAME
#warning "DVEC_LOWERCASE_NAME is not defined"
#define DVEC_LOWERCASE_NAME placeholder_vec
#endif // DVEC_LOWERCASE_NAME

#ifndef DVEC_TYPE
#warning "DVEC_TYPE is not defined"
#define DVEC_TYPE PlaceholderVec
#endif // DVEC_TYPE

// Helper macros
#define _CAT(a, b) a##b
#define CAT(a, b) _CAT(a, b)
#define DVEC_FN(name) CAT(DVEC_LOWERCASE_NAME, CAT(_, name))

// An arena-backed growable vector
typedef struct DVEC_NAME DVEC_NAME;

// Creates a new arena-backed vector with zero elements
DVEC_NAME* DVEC_FN(new)(Arena* arena);

// Pushes an element to the vector
DVEC_TYPE* DVEC_FN(push)(DVEC_NAME* vec, DVEC_TYPE element);

// Gets the element stored at index `idx`, storing the element in `out` and
// returning a bool indicating whether the operation was successful.
bool DVEC_FN(get)(DVEC_NAME* vec, size_t idx, DVEC_TYPE* out);

// Gets the element stored at index `idx`, storing a pointer to the element in
// `out` and returning a bool indicating whether the operation was successful.
bool DVEC_FN(get_ptr)(DVEC_NAME* vec, size_t idx, DVEC_TYPE** out);

// Pops an element from the vector, storing the pop'ed element in `out` and
// returning a bool indicating whether the operation was successful.
bool DVEC_FN(pop)(DVEC_NAME* vec, DVEC_TYPE* out);

// Clears the vector, making it's length zero. This does not free any memory.
void DVEC_FN(clear)(DVEC_NAME* vec);

// Returns the length of the vector.
size_t DVEC_FN(len)(DVEC_NAME* vec);

// Undefines
#undef DVEC_NAME
#undef DVEC_LOWERCASE_NAME
#undef DVEC_TYPE
#undef _CAT
#undef CAT
#undef DVEC_FN
