#include "filters.h"

#include <string.h>

#include "arena/arena.h"
#include "logger/log.h"

char* pdf_decode_filtered_stream(
    Arena* arena,
    const char* encoded,
    size_t length,
    PdfOpNameArray filters
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(encoded);

    if (filters.discriminant) {
        return NULL;
    } else {
        char* stream_body =
            arena_alloc(arena, sizeof(char) * (size_t)(length + 1));
        strcpy(stream_body, encoded);

        return stream_body;
    }
}
