#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "arena/arena.h"
#include "logger/log.h"
#include "pdf_error/error.h"
#include "sfnt/glyph.h"
#include "sfnt/sfnt.h"

static uint8_t*
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

    uint8_t* buffer = arena_alloc(arena, (size_t)len);
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

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    Arena* arena = arena_new(4096);

    size_t buffer_len;
    uint8_t* buffer = load_file_to_buffer(
        arena,
        "test-files/LiberationMono-Regular.ttf",
        &buffer_len
    );

    SfntFont* font;
    PDF_REQUIRE(sfnt_font_new(arena, buffer, buffer_len, &font));

    const char* test_text = "Hello World!";
    for (size_t idx = 0; idx < strlen(test_text); idx++) {
        uint32_t cid = (uint32_t)test_text[idx];

        SfntGlyph glyph;
        PDF_REQUIRE(sfnt_get_glyph(font, cid, &glyph));
    }

    LOG_DIAG(INFO, EXAMPLE, "Finished");

    arena_free(arena);
    return 0;
}
