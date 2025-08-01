#ifndef DLINKED_H
#define DLINKED_H

#include "arena/arena.h"

#endif // DLINKED_H

// Check arguments
#ifndef DLINKED_NAME
#warning "DLINKED_NAME is not defined"
#define DLINKED_NAME PlaceholderArray
#endif // DLINKED_NAME

#ifndef DLINKED_LOWERCASE_NAME
#warning "DLINKED_LOWERCASE_NAME is not defined"
#define DLINKED_LOWERCASE_NAME placeholder_array
#endif // DLINKED_LOWERCASE_NAME

#ifndef DLINKED_TYPE
#warning "DLINKED_TYPE is not defined"
#define DLINKED_TYPE int
#endif // DLINKED_TYPE

// Helper macros
#define _CAT(a, b) a##b
#define CAT(a, b) _CAT(a, b)
#define DLINKED_FN(name) CAT(DLINKED_LOWERCASE_NAME, CAT(_, name))

// A doubly-linked list
typedef struct DLINKED_NAME DLINKED_NAME;

// Creates a new linked-list with no elements
DLINKED_NAME* DLINKED_FN(new)(Arena* arena);

// Sets the search cursor to a given index. This will make future operations
// near this index faster. This function will panic if idx is out of bounds.
void DLINKED_FN(set_cursor)(DLINKED_NAME* linked_list, size_t idx);

// Gets the element stored at index `idx` and stores it in `out`. If the index
// is out of bounds, false is returned and `out` isn't set.
bool DLINKED_FN(get)(DLINKED_NAME* linked_list, size_t idx, DLINKED_TYPE* out);

// Gets the element stored at index `idx` and stores a pointer to it in `out`.
// If the index is out of bounds, false is returned and `out` isn't set.
bool DLINKED_FN(get_ptr)(
    DLINKED_NAME* linked_list,
    size_t idx,
    DLINKED_TYPE** out
);

// Sets the element stored at index `idx`, which must be in bounds.
void DLINKED_FN(set)(DLINKED_NAME* linked_list, size_t idx, DLINKED_TYPE value);

// Inserts an element at the index `idx`. Panics if the index is out of bounds.
void DLINKED_FN(insert)(
    DLINKED_NAME* linked_list,
    size_t idx,
    DLINKED_TYPE element
);

// Inserts an element at the index `idx`, returning the data. Panics if the
// index is out of bounds.
DLINKED_TYPE DLINKED_FN(remove)(DLINKED_NAME* linked_list, size_t idx);

// Inserts an element the start of the list
void DLINKED_FN(push_front)(DLINKED_NAME* linked_list, DLINKED_TYPE element);

// Inserts an element the end of the list
void DLINKED_FN(push_back)(DLINKED_NAME* linked_list, DLINKED_TYPE element);

// Pops an element the start of the list, storing the pop'ed element in `out`
// and returning a bool indicating whether the operation was successful.
bool DLINKED_FN(pop_front)(DLINKED_NAME* linked_list, DLINKED_TYPE* out);

// Pops an element the back of the list, storing the pop'ed element in `out`
// and returning a bool indicating whether the operation was successful.
bool DLINKED_FN(pop_back)(DLINKED_NAME* linked_list, DLINKED_TYPE* out);

// Inserts an element into a sorted linked list. If the list is not sorted, the
// behavior is undefined. `cmp_less_than` should return `true` when lhs is less
// than rhs. Returns the index which the element was inserted into.
size_t DLINKED_FN(insert_sorted)(
    DLINKED_NAME* linked_list,
    DLINKED_TYPE element,
#ifndef DLINKED_SORT_USER_DATA
    bool (*cmp_less_than)(DLINKED_TYPE* lhs, DLINKED_TYPE* rhs),
#else
    bool (*cmp_less_than)(
        DLINKED_TYPE* lhs,
        DLINKED_TYPE* rhs,
        DLINKED_SORT_USER_DATA user_data
    ),
    DLINKED_SORT_USER_DATA user_data,
#endif
    bool ascending
);

// Merges `other` into `linked_list`, such that `linked_list` is sorted,
// assuming both input lists were already sorted. If `other` or `linked_list`
// were not sorted, the behavior is undefined.
void DLINKED_FN(merge_sorted)(
    DLINKED_NAME* linked_list,
    DLINKED_NAME* other,
#ifndef DLINKED_SORT_USER_DATA
    bool (*cmp_less_than)(DLINKED_TYPE* lhs, DLINKED_TYPE* rhs),
#else
    bool (*cmp_less_than)(
        DLINKED_TYPE* lhs,
        DLINKED_TYPE* rhs,
        DLINKED_SORT_USER_DATA user_data
    ),
    DLINKED_SORT_USER_DATA user_data,
#endif
    bool ascending
);

// Clears the linked-list, making it's length zero. This does not free any
// memory.
void DLINKED_FN(clear)(DLINKED_NAME* linked_list);

// Gets the length of the linked list
size_t DLINKED_FN(len)(DLINKED_NAME* linked_list);

// Undefines
#undef DLINKED_NAME
#undef DLINKED_LOWERCASE_NAME
#undef DLINKED_TYPE
#undef _CAT
#undef CAT
#undef DLINKED_FN
