#include <stdbool.h>

#include "arena/arena.h"
#include "logger/log.h"

// Check arguments
#ifndef DLINKED_NAME
#error "DLINKED_NAME is not defined"
#define DLINKED_NAME PlaceholderArray
#endif // DLINKED_NAME

#ifndef DLINKED_LOWERCASE_NAME
#error "DLINKED_LOWERCASE_NAME is not defined"
#define DLINKED_LOWERCASE_NAME placeholder_array
#endif // DLINKED_LOWERCASE_NAME

#ifndef DLINKED_TYPE
#error "DLINKED_TYPE is not defined"
#define DLINKED_TYPE int
#endif // DLINKED_TYPE

// Helper macros
#define _CAT(a, b) a##b
#define CAT(a, b) _CAT(a, b)
#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)
#define DLINKED_FN(name) CAT(DLINKED_LOWERCASE_NAME, CAT(_, name))

// A doubly-linked list
typedef struct DLINKED_NAME DLINKED_NAME;

#define DLINKED_BLOCK_NAME CAT(DLINKED_NAME, Block)

typedef struct DLINKED_BLOCK_NAME {
    struct DLINKED_BLOCK_NAME* prev;
    struct DLINKED_BLOCK_NAME* next;
    DLINKED_TYPE data;
} DLINKED_BLOCK_NAME;

struct DLINKED_NAME {
    size_t len;
    Arena* arena;

    DLINKED_BLOCK_NAME* front;
    DLINKED_BLOCK_NAME* back;

    size_t cursor_idx;
    DLINKED_BLOCK_NAME* cursor;

    DLINKED_BLOCK_NAME* pool;
};

// Creates a new linked-list with no elements
DLINKED_NAME* DLINKED_FN(new)(Arena* arena) {
    RELEASE_ASSERT(arena);

    LOG_DIAG(
        INFO,
        LINKED_LIST,
        "Creating new " STRINGIFY(DLINKED_NAME
        ) " (DoublyLinkedList<" STRINGIFY(DLINKED_TYPE) ">)"
    );

    DLINKED_NAME* linked_list = arena_alloc(arena, sizeof(DLINKED_NAME));
    linked_list->arena = arena;
    linked_list->len = 0;
    linked_list->front = NULL;
    linked_list->back = NULL;
    linked_list->cursor_idx = 0;
    linked_list->cursor = NULL;
    linked_list->pool = NULL;

    return linked_list;
}

// Sets the search cursor to a given index. This will make future operations
// near this index faster. This function will panic if idx is out of bounds.
void DLINKED_FN(set_cursor)(DLINKED_NAME* linked_list, size_t idx) {
    RELEASE_ASSERT(linked_list);
    RELEASE_ASSERT(idx < linked_list->len);

    if (!linked_list->cursor) {
        linked_list->cursor_idx = 0;
        linked_list->cursor = linked_list->front;
    }

    LOG_DIAG(TRACE, LINKED_LIST, "Moving cursor to idx %zu", idx);

    // Move cursor to best starting point
    if (linked_list->cursor && linked_list->cursor_idx > idx
        && linked_list->cursor_idx - idx > idx) {
        LOG_DIAG(TRACE, LINKED_LIST, "Searching from front");
        linked_list->cursor_idx = 0;
        linked_list->cursor = linked_list->front;
    }

    if (linked_list->cursor && linked_list->cursor_idx < idx
        && idx - linked_list->cursor_idx > linked_list->len - 1 - idx) {
        LOG_DIAG(TRACE, LINKED_LIST, "Searching from back");
        linked_list->cursor_idx = linked_list->len - 1;
        linked_list->cursor = linked_list->back;
    }

    // Move cursor to location
    while (linked_list->cursor_idx > idx) {
        RELEASE_ASSERT(linked_list->cursor);

        linked_list->cursor_idx--;
        linked_list->cursor = linked_list->cursor->prev;
    }

    while (linked_list->cursor_idx < idx) {
        RELEASE_ASSERT(linked_list->cursor);

        linked_list->cursor_idx++;
        linked_list->cursor = linked_list->cursor->next;
    }
}

// Gets the element stored at index `idx` and stores it in `out`. If the index
// is out of bounds, false is returned and `out` isn't set.
bool DLINKED_FN(get)(DLINKED_NAME* linked_list, size_t idx, DLINKED_TYPE* out) {
    RELEASE_ASSERT(linked_list);
    RELEASE_ASSERT(out);

    LOG_DIAG(
        DEBUG,
        LINKED_LIST,
        "Getting " STRINGIFY(DLINKED_TYPE
        ) " element at idx %zu from " STRINGIFY(DLINKED_NAME),
        idx
    );

    if (idx >= linked_list->len) {
        LOG_DIAG(TRACE, LINKED_LIST, "Out-of-bounds");
        return false;
    }

    DLINKED_FN(set_cursor)(linked_list, idx);
    *out = linked_list->cursor->data;
    return true;
}

// Gets the element stored at index `idx` and stores a pointer to it in `out`.
// If the index is out of bounds, false is returned and `out` isn't set.
bool DLINKED_FN(get_ptr)(
    DLINKED_NAME* linked_list,
    size_t idx,
    DLINKED_TYPE** out
) {
    RELEASE_ASSERT(linked_list);
    RELEASE_ASSERT(out);

    LOG_DIAG(
        DEBUG,
        LINKED_LIST,
        "Getting " STRINGIFY(DLINKED_TYPE
        ) " element ptr at idx %zu from " STRINGIFY(DLINKED_NAME),
        idx
    );

    if (idx >= linked_list->len) {
        LOG_DIAG(TRACE, LINKED_LIST, "Out-of-bounds");
        return false;
    }

    DLINKED_FN(set_cursor)(linked_list, idx);
    *out = &linked_list->cursor->data;
    return true;
}

// Gets the last element of the linked-list and stores it in `out`. If the list
// is empty, false is returned and `out` isn't set.
bool DLINKED_FN(back)(const DLINKED_NAME* linked_list, DLINKED_TYPE* out) {
    RELEASE_ASSERT(linked_list);
    RELEASE_ASSERT(out);

    LOG_DIAG(
        DEBUG,
        LINKED_LIST,
        "Getting last " STRINGIFY(DLINKED_TYPE
        ) " element from " STRINGIFY(DLINKED_NAME)
    );

    if (linked_list->len == 0) {
        LOG_DIAG(TRACE, LINKED_LIST, "List empty");
        return false;
    }

    *out = linked_list->back->data;

    return true;
}

// Sets the element stored at index `idx`, which must be in bounds.
void DLINKED_FN(set)(
    DLINKED_NAME* linked_list,
    size_t idx,
    DLINKED_TYPE value
) {
    RELEASE_ASSERT(linked_list);
    RELEASE_ASSERT(idx < linked_list->len);

    LOG_DIAG(
        DEBUG,
        LINKED_LIST,
        "Setting " STRINGIFY(DLINKED_TYPE
        ) " element at idx %zu in " STRINGIFY(DLINKED_NAME),
        idx
    );

    DLINKED_FN(set_cursor)(linked_list, idx);
    linked_list->cursor->data = value;
}

// Inserts an element at the index `idx`. Panics if the index is out of bounds.
void DLINKED_FN(insert)(
    DLINKED_NAME* linked_list,
    size_t idx,
    DLINKED_TYPE element
) {
    RELEASE_ASSERT(linked_list);
    RELEASE_ASSERT(idx <= linked_list->len);

    LOG_DIAG(
        DEBUG,
        LINKED_LIST,
        "Inserting " STRINGIFY(DLINKED_TYPE
        ) " element at idx %zu in " STRINGIFY(DLINKED_NAME),
        idx
    );

    // Get block
    DLINKED_BLOCK_NAME* block = NULL;
    if (linked_list->pool) {
        LOG_DIAG(TRACE, LINKED_LIST, "Using pool block");

        block = linked_list->pool;
        linked_list->pool = block->next;

        block->prev = NULL;
        block->next = NULL;
    } else {
        LOG_DIAG(TRACE, LINKED_LIST, "Allocating block");
        block = arena_alloc(linked_list->arena, sizeof(DLINKED_BLOCK_NAME));
    }

    // Insert
    if (idx == linked_list->len) {
        LOG_DIAG(TRACE, LINKED_LIST, "Insertion is at end of list");

        // End of list
        block->prev = linked_list->back;
        block->next = NULL;
        block->data = element;
        linked_list->cursor_idx = idx;
        linked_list->cursor = block;
        linked_list->back = block;

        if (block->prev) {
            block->prev->next = block;
        } else {
            linked_list->front = block;
        }
    } else {
        // Within list
        DLINKED_FN(set_cursor)(linked_list, idx);
        RELEASE_ASSERT(linked_list->cursor);

        block->prev = linked_list->cursor->prev;
        block->next = linked_list->cursor;
        block->data = element;
        linked_list->cursor_idx = idx;
        linked_list->cursor = block;

        // Set neighbours
        if (block->prev) {
            block->prev->next = block;
        } else {
            linked_list->front = block;
        }

        if (block->next) {
            block->next->prev = block;
        }
    }

    linked_list->len++;
}

// Inserts an element at the index `idx`, returning the data. Panics if the
// index is out of bounds.
DLINKED_TYPE DLINKED_FN(remove)(DLINKED_NAME* linked_list, size_t idx) {
    RELEASE_ASSERT(linked_list);
    RELEASE_ASSERT(idx < linked_list->len);

    LOG_DIAG(
        DEBUG,
        LINKED_LIST,
        "Removing " STRINGIFY(DLINKED_TYPE
        ) " element from idx %zu in " STRINGIFY(DLINKED_NAME),
        idx
    );

    DLINKED_FN(set_cursor)(linked_list, idx);
    DLINKED_BLOCK_NAME* block = linked_list->cursor;

    // Unlink
    if (block->prev) {
        block->prev->next = block->next;
    } else {
        linked_list->front = block->next;
    }

    if (block->next) {
        block->next->prev = block->prev;
        linked_list->cursor = block->next;
    } else {
        linked_list->back = block->prev;
    }

    // Update cursor
    if (block->next) {
        linked_list->cursor = block->next;
    } else if (!block->next && block->prev) {
        linked_list->cursor_idx--;
        linked_list->cursor = block->prev;
    } else {
        linked_list->cursor_idx = 0;
        linked_list->cursor = NULL;
    }

    // Add block to pool
    block->next = linked_list->pool;
    linked_list->pool = block;

    linked_list->len--;
    return block->data;
}

// Inserts an element the start of the list
void DLINKED_FN(push_front)(DLINKED_NAME* linked_list, DLINKED_TYPE element) {
    RELEASE_ASSERT(linked_list);

    DLINKED_FN(insert)(linked_list, 0, element);
}

// Inserts an element the end of the list
void DLINKED_FN(push_back)(DLINKED_NAME* linked_list, DLINKED_TYPE element) {
    DLINKED_FN(insert)(linked_list, linked_list->len, element);
}

// Pops an element the start of the list, storing the pop'ed element in `out`
// and returning a bool indicating whether the operation was successful.
bool DLINKED_FN(pop_front)(DLINKED_NAME* linked_list, DLINKED_TYPE* out) {
    RELEASE_ASSERT(linked_list);

    if (linked_list->len == 0) {
        return false;
    }

    DLINKED_TYPE value = DLINKED_FN(remove)(linked_list, 0);
    if (out) {
        *out = value;
    }
    return true;
}

// Pops an element the back of the list, storing the pop'ed element in `out`
// and returning a bool indicating whether the operation was successful.
bool DLINKED_FN(pop_back)(DLINKED_NAME* linked_list, DLINKED_TYPE* out) {
    RELEASE_ASSERT(linked_list);

    if (linked_list->len == 0) {
        return false;
    }

    DLINKED_TYPE value = DLINKED_FN(remove)(linked_list, linked_list->len - 1);
    if (out) {
        *out = value;
    }
    return true;
}

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
) {
    RELEASE_ASSERT(linked_list);
    RELEASE_ASSERT(cmp_less_than);

    LOG_DIAG(
        DEBUG,
        LINKED_LIST,
        "Inserting " STRINGIFY(DLINKED_TYPE
        ) " element into sorted " STRINGIFY(DLINKED_TYPE) " list in %s",
        ascending ? "ascending order" : "descending order"
    );

    // Handle zero-length
    if (linked_list->len == 0) {
        DLINKED_FN(insert)(linked_list, 0, element);
        return 0;
    }

    // Reset cursor to the start of the list if the insertion point is before
    // the cursor
    if (linked_list->cursor
        && cmp_less_than(
               &element,
               &linked_list->cursor->data
#ifdef DLINKED_SORT_USER_DATA
               ,
               user_data
#endif
           ) == ascending) {
        linked_list->cursor_idx = 0;
        linked_list->cursor = linked_list->front;
    }

    RELEASE_ASSERT(linked_list->cursor);

    // Find insertion idx
    while (linked_list->cursor->next
           && (cmp_less_than(
                   &element,
                   &linked_list->cursor->data
#ifdef DLINKED_SORT_USER_DATA
                   ,
                   user_data
#endif
               )
               != ascending)) {
        linked_list->cursor_idx++;
        linked_list->cursor = linked_list->cursor->next;
    }

    size_t insert_idx = linked_list->cursor_idx;
    if (linked_list->cursor->next == NULL
        && (cmp_less_than(
                &linked_list->cursor->data,
                &element
#ifdef DLINKED_SORT_USER_DATA
                ,
                user_data
#endif
            )
            == ascending)) {
        insert_idx = linked_list->len;
    }

    // Insert
    DLINKED_FN(insert)(linked_list, insert_idx, element);
    return insert_idx;
}

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
) {
    RELEASE_ASSERT(linked_list);
    RELEASE_ASSERT(other);
    RELEASE_ASSERT(cmp_less_than);

    LOG_DIAG(
        INFO,
        LINKED_LIST,
        "Merging sorted " STRINGIFY(DLINKED_NAME) " lists in %s",
        ascending ? "ascending order" : "descending order"
    );

    if (other->len == 0) {
        return;
    }

    // Initialize local cursors
    size_t current_index = 0;
    DLINKED_BLOCK_NAME* current_node = linked_list->front;
    DLINKED_BLOCK_NAME* other_node = other->front;

    // Iterate through each element in other
    while (other_node) {
        // Find insertion position in linked_list
        while (current_node
               && cmp_less_than(
                      &current_node->data,
                      &other_node->data
#ifdef DLINKED_SORT_USER_DATA
                      ,
                      user_data
#endif
                  ) == ascending) {
            current_node = current_node->next;
            current_index++;
        }

        // Insert the element at the found position
        DLINKED_FN(insert)(linked_list, current_index, other_node->data);

        // Update local cursor
        if (current_node != NULL) {
            current_index++;
        } else {
            current_index = linked_list->len;
        }

        // Move to next element in other
        other_node = other_node->next;
    }
}

// Clears the linked-list, making it's length zero. This does not free any
// memory.
void DLINKED_FN(clear)(DLINKED_NAME* linked_list) {
    RELEASE_ASSERT(linked_list);

    LOG_DIAG(
        DEBUG,
        LINKED_LIST,
        "Clearing " STRINGIFY(DLINKED_NAME) " linked list with %zu elements",
        linked_list->len
    );

    if (linked_list->len == 0) {
        return;
    }

    linked_list->back->next = linked_list->pool;
    linked_list->pool = linked_list->front;
    linked_list->front = NULL;
    linked_list->back = NULL;

    linked_list->len = 0;

    linked_list->cursor_idx = 0;
    linked_list->cursor = NULL;
}

// Gets the length of the linked list
size_t DLINKED_FN(len)(const DLINKED_NAME* linked_list) {
    RELEASE_ASSERT(linked_list);
    return linked_list->len;
}

// Undefines
#undef DLINKED_NAME
#undef DLINKED_LOWERCASE_NAME
#undef DLINKED_TYPE
#undef _CAT
#undef CAT
#undef _STRINGIFY
#undef STRINGIFY
#undef DLINKED_FN
#undef DLINKED_BLOCK_NAME
#undef DLINKED_SORT_USER_DATA
