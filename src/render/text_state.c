#include "text_state.h"

#include <stdio.h>
#include <string.h>

#include "geom/mat3.h"
#include "logger/log.h"
#include "sfnt/sfnt.h"

TextState text_state_default(void) {
    return (TextState
    ) {.font_set = false,
       .character_spacing = 0.0,
       .word_spacing = 0.0,
       .horizontal_spacing = 1.0};
}

TextObjectState text_object_state_default(void) {
    return (TextObjectState
    ) {.text_matrix = geom_mat3_identity(),
       .text_line_matrix = geom_mat3_identity()};
}

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

PdfError* text_state_render(
    Arena* arena,
    Canvas* canvas,
    GeomMat3 ctm,
    TextState* state,
    TextObjectState* object_state,
    const char* data
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(state);
    RELEASE_ASSERT(data);

    size_t buffer_len;
    uint8_t* buffer = load_file_to_buffer(
        arena,
        "assets/fonts-urw-base35/fonts/NimbusRoman-Regular.ttf",
        &buffer_len
    );

    SfntFont* font;
    PDF_REQUIRE(sfnt_font_new(arena, buffer, buffer_len, &font));

    for (size_t cid_idx = 0; cid_idx < strlen(data); cid_idx++) {
        SfntGlyph glyph;
        PDF_PROPAGATE(sfnt_get_glyph(font, (uint32_t)data[cid_idx], &glyph));

        GeomMat3 render_matrix = geom_mat3_mul(
            geom_mat3_mul(
                geom_mat3_new(
                    state->text_font_size * state->horizontal_spacing * 0.001,
                    0.0,
                    0.0,
                    0.0,
                    state->text_font_size * 0.001,
                    0.0,
                    0.0,
                    state->text_rise,
                    1.0
                ),
                object_state->text_matrix
            ),
            ctm
        );

        sfnt_glyph_render(canvas, &glyph, render_matrix);

        // TODO: support vertical fonts
        double t_x = ((double)glyph.advance_width * state->text_font_size
                      + state->character_spacing + state->word_spacing)
                   * state->horizontal_spacing * 0.001;
        object_state->text_matrix = geom_mat3_mul(
            object_state->text_matrix,
            geom_mat3_translate(t_x, 0.0)
        );
    }

    return NULL;
}
