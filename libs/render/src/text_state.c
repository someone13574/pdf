#include "text_state.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "font.h"
#include "geom/mat3.h"
#include "logger/log.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"

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

PdfError* text_state_render(
    Arena* arena,
    Canvas* canvas,
    PdfOptionalResolver resolver,
    PdfCMapCache* cmap_cache,
    GeomMat3 ctm,
    TextState* state,
    TextObjectState* object_state,
    PdfString text
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(canvas);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));
    RELEASE_ASSERT(cmap_cache);
    RELEASE_ASSERT(state);
    RELEASE_ASSERT(object_state);
    RELEASE_ASSERT(text.data);

    if (!state->font_set) {
        return PDF_ERROR(PDF_ERR_RENDER_FONT_NOT_SET);
    }

    size_t offset = 0;

    while (true) {
        // Get CID
        bool finished = false;
        uint32_t cid;
        PDF_PROPAGATE(next_cid(
            &state->text_font,
            cmap_cache,
            &text,
            &offset,
            &finished,
            &cid
        ));

        if (finished) {
            break;
        }

        // Get GID
        uint32_t gid;
        PDF_PROPAGATE(cid_to_gid(&state->text_font, resolver, cid, &gid));

        // Render
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

        PDF_PROPAGATE(render_glyph(
            &state->text_font,
            resolver,
            gid,
            canvas,
            render_matrix
        ));

        // TODO: support vertical fonts
        double t_x = ((double)500.0 * state->text_font_size
                      + state->character_spacing + state->word_spacing)
                   * state->horizontal_spacing * 0.001;
        object_state->text_matrix = geom_mat3_mul(
            object_state->text_matrix,
            geom_mat3_translate(t_x, 0.0)
        );
    }

    return NULL;
}
