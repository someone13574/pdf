#pragma once

#ifdef TEST

#include <stddef.h>

#include "arena.h"

char* pdf_construct_deserde_test_doc(
    const char** objects,
    size_t num_objects,
    const char* trailer_dict,
    Arena* arena
);

#endif // TEST
