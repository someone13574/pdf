#include "test_helpers.h"

#include "logger/log.h"

#ifdef TEST

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "arena/arena.h"

static char* format_alloc(Arena* arena, const char* fmt, ...)
    __attribute__((format(printf, 2, 3)));

static char* format_alloc(Arena* arena, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, fmt, args_copy);
    char* buffer = arena_alloc(arena, (size_t)needed + 1);
    vsprintf(buffer, fmt, args);
    va_end(args);

    return buffer;
}

char* pdf_construct_deserde_test_doc(
    const char** objects,
    size_t num_objects,
    const char* trailer_dict,
    Arena* arena
) {
    RELEASE_ASSERT(num_objects <= 1024);

    char* doc = "%PDF-1.1\n";
    size_t cursor = strlen(doc);
    size_t offsets[num_objects];

    // Add objects
    for (size_t idx = 0; idx < num_objects; idx++) {
        offsets[idx] = cursor;

        char* indirect_obj =
            format_alloc(arena, "%zu 0 obj %s endobj\n", idx + 1, objects[idx]);
        RELEASE_ASSERT(indirect_obj);
        cursor += strlen(indirect_obj);

        doc = format_alloc(arena, "%s%s", doc, indirect_obj);
    }

    size_t startxref = cursor + 1;

    // Create xref
    doc = format_alloc(
        arena,
        "%s\nxref\n0 %zu\n0000000000 65535 f \n",
        doc,
        num_objects + 1
    );
    for (size_t idx = 0; idx < num_objects; idx++) {
        doc = format_alloc(arena, "%s%010zu 00000 n \n", doc, offsets[idx]);
    }

    // Trailer
    doc = format_alloc(
        arena,
        "%strailer\n%s\nstartxref\n%zu\n%%%%EOF\n",
        doc,
        trailer_dict,
        startxref
    );
    return doc;
}

#endif // TEST
