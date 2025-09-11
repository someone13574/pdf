#include "pdf/fonts/cmap.h"

#include <stdint.h>
#include <stdio.h>

#include "arena/arena.h"
#include "logger/log.h"
#include "pdf_error/error.h"

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

    PdfCMap* cmap = NULL;
    PDF_REQUIRE(pdf_parse_cmap(arena, buffer, buffer_len, &cmap));

    for (uint32_t codepoint = 0; codepoint <= 0xffff; codepoint++) {
        uint32_t cid;
        PDF_REQUIRE(
            pdf_cmap_get_cid(cmap, codepoint, &cid),
            "Failed to get cid for codepoint 0x%04x",
            (unsigned int)codepoint
        );

        LOG_DIAG(
            INFO,
            CMAP,
            "0x%04x = %llu",
            codepoint,
            (unsigned long long int)cid
        );
    }

    arena_free(arena);
    return 0;
}
