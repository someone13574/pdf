#include "text_state.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "font.h"
#include "geom/mat3.h"
#include "logger/log.h"
#include "pdf/object.h"
#include "pdf_error/error.h"
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

PdfError* text_state_render(
    Arena* arena,
    Canvas* canvas,
    GeomMat3 ctm,
    TextState* state,
    TextObjectState* object_state,
    PdfString text
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(canvas);
    RELEASE_ASSERT(state);
    RELEASE_ASSERT(object_state);
    RELEASE_ASSERT(text.data);

    (void)ctm;

    if (!state->font_set) {
        return PDF_ERROR(PDF_ERR_RENDER_FONT_NOT_SET);
    }

    size_t offset = 0;
    bool finished = false;

    while (!finished) {
        uint32_t cid = next_cid(&state->text_font, &text, &offset, &finished);
        LOG_WARN(RENDER, "Cid: %u", (unsigned int)cid);
    }

    // size_t buffer_len;
    // uint8_t* buffer = load_file_to_buffer(
    //     arena,
    //     "assets/fonts-urw-base35/fonts/NimbusRoman-Regular.ttf",
    //     &buffer_len
    // );

    // SfntFont* font;
    // PDF_REQUIRE(sfnt_font_new(arena, buffer, buffer_len, &font));

    // for (size_t cid_idx = 0; cid_idx < text.len; cid_idx++) {
    //     SfntGlyph glyph;
    //     PDF_PROPAGATE(sfnt_get_glyph(font, (uint32_t)text.data[cid_idx],
    //     &glyph)
    //     );

    //     GeomMat3 render_matrix = geom_mat3_mul(
    //         geom_mat3_mul(
    //             geom_mat3_new(
    //                 state->text_font_size * state->horizontal_spacing *
    //                 0.001, 0.0, 0.0, 0.0, state->text_font_size * 0.001, 0.0,
    //                 0.0,
    //                 state->text_rise,
    //                 1.0
    //             ),
    //             object_state->text_matrix
    //         ),
    //         ctm
    //     );

    //     sfnt_glyph_render(canvas, &glyph, render_matrix);

    //     // TODO: support vertical fonts
    //     double t_x = ((double)glyph.advance_width * state->text_font_size
    //                   + state->character_spacing + state->word_spacing)
    //                * state->horizontal_spacing * 0.001;
    //     object_state->text_matrix = geom_mat3_mul(
    //         object_state->text_matrix,
    //         geom_mat3_translate(t_x, 0.0)
    //     );
    // }

    return NULL;
}
