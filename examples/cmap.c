#include "pdf/fonts/cmap.h"

#include <stdio.h>

#include "arena/arena.h"

static char*
load_file_to_buffer(Arena* arena, const char* path, size_t* out_size) {
    FILE* file = fopen(path, "rb");
    *out_size = 0;
    if (!file) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    long len = ftell(file);
    if (len < 0) {
        fclose(file);
        return NULL;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    char* buffer = arena_alloc(arena, (size_t)len);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    if (fread(buffer, 1, (size_t)len, file) != (size_t)len) {
        fclose(file);
        return NULL;
    }
    fclose(file);

    *out_size = (size_t)len;
    return buffer;
}

int main(void) {
    Arena* arena = arena_new(1024);

    size_t buffer_len;
    const char* buffer = load_file_to_buffer(
        arena,
        "assets/cmap-resources/Adobe-Identity-0/CMap/Identity-H",
        &buffer_len
    );

    PDF_REQUIRE(pdf_parse_cmap(arena, buffer, buffer_len));

    arena_free(arena);
    return 0;
}
