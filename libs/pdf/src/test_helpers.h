#pragma once

#include "ctx.h"
#include "pdf/resolver.h"
#ifdef TEST

#include <stddef.h>

#include "arena/arena.h"

PdfResolver* pdf_fake_resolver_new(Arena* arena, PdfCtx* ctx);

char* pdf_construct_deserde_test_doc(
    const char** objects,
    size_t num_objects,
    const char* trailer_dict,
    Arena* arena
);

#endif // TEST
