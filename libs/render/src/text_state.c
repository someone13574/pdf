#include "text_state.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "cache.h"
#include "font.h"
#include "geom/mat3.h"
#include "logger/log.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"

TextState text_state_default(void) {
    return (TextState) {.font_set = false,
                        .character_spacing = 0.0,
                        .word_spacing = 0.0,
                        .horizontal_scaling = 1.0};
}

TextObjectState text_object_state_default(void) {
    return (TextObjectState) {.text_matrix = geom_mat3_identity(),
                              .text_line_matrix = geom_mat3_identity()};
}

PdfError* text_state_render(
    Arena* arena,
    Canvas* canvas,
    PdfResolver* resolver,
    RenderCache* cache,
    GeomMat3 ctm,
    TextState* state,
    TextObjectState* object_state,
    PdfString text,
    CanvasBrush brush
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(canvas);
    RELEASE_ASSERT(resolver);
    RELEASE_ASSERT(cache);
    RELEASE_ASSERT(state);
    RELEASE_ASSERT(object_state);
    RELEASE_ASSERT(text.data);

    if (!state->font_set) {
        return PDF_ERROR(RENDER_ERR_FONT_NOT_SET);
    }

    size_t offset = 0;

    while (true) {
        // Get CID
        bool finished = false;
        uint32_t cid;
        PDF_PROPAGATE(
            next_cid(&state->text_font, cache, &text, &offset, &finished, &cid)
        );

        if (finished) {
            break;
        }

        // Get GID
        uint32_t gid;
        PDF_PROPAGATE(
            cid_to_gid(arena, &state->text_font, cache, resolver, cid, &gid)
        );

        // Get font matrix
        GeomMat3 font_matrix;
        PDF_PROPAGATE(
            get_font_matrix(arena, resolver, &state->text_font, &font_matrix)
        );

        // Render
        GeomMat3 render_matrix = geom_mat3_mul(
            geom_mat3_mul(
                geom_mat3_mul(
                    geom_mat3_new(
                        state->text_font_size * state->horizontal_scaling,
                        0.0,
                        0.0,
                        0.0,
                        state->text_font_size,
                        0.0,
                        0.0,
                        state->text_rise,
                        1.0
                    ),
                    font_matrix
                ),
                object_state->text_matrix
            ),
            ctm
        );

        PDF_PROPAGATE(render_glyph(
            arena,
            &state->text_font,
            resolver,
            gid,
            canvas,
            render_matrix,
            brush
        ));

        PdfNumber glyph_width;
        PDF_PROPAGATE(
            cid_to_width(&state->text_font, resolver, cid, &glyph_width)
        );

        double tx =
            (pdf_number_as_real(glyph_width) * 0.001 * state->text_font_size
             + state->character_spacing)
            * state->horizontal_scaling;
        object_state->text_matrix = geom_mat3_mul(
            geom_mat3_translate(tx, 0.0),
            object_state->text_matrix
        );
    }

    return NULL;
}
