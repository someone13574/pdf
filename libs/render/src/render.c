#include "render/render.h"

#include "arena/arena.h"
#include "canvas/canvas.h"
#include "geom/mat3.h"
#include "geom/rect.h"
#include "geom/vec2.h"
#include "graphics_state.h"
#include "logger/log.h"
#include "pdf/content_op.h"
#include "pdf/content_stream.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf/resources/font.h"
#include "pdf/types.h"
#include "pdf_error/error.h"
#include "text_state.h"

#define SCALE (25.4 / 72.0) // TODO: Load user-space units for conversion factor

typedef struct {
    GraphicsState graphics_state;
    TextObjectState text_object_state;
} RenderState;

static PdfError* process_content_stream(
    Arena* arena,
    RenderState* state,
    PdfContentStream* content_stream,
    const PdfOpResources* resources,
    PdfOptionalResolver resolver,
    Canvas* canvas
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(state);
    RELEASE_ASSERT(content_stream);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));
    RELEASE_ASSERT(canvas);

    for (size_t idx = 0;
         idx < pdf_content_op_vec_len(content_stream->operations);
         idx++) {
        PdfContentOp op;
        RELEASE_ASSERT(
            pdf_content_op_vec_get(content_stream->operations, idx, &op)
        );

        switch (op.kind) {
            case PDF_CONTENT_OP_SET_CTM: {
                state->graphics_state.ctm = geom_mat3_new_pdf(
                    pdf_number_as_real(op.data.set_matrix.a),
                    pdf_number_as_real(op.data.set_matrix.b),
                    pdf_number_as_real(op.data.set_matrix.c),
                    pdf_number_as_real(op.data.set_matrix.d),
                    pdf_number_as_real(op.data.set_matrix.e),
                    pdf_number_as_real(op.data.set_matrix.f)
                );
                break;
            }
            case PDF_CONTENT_OP_BEGIN_TEXT: {
                state->text_object_state = text_object_state_default();
                break;
            }
            case PDF_CONTENT_OP_END_TEXT: {
                break;
            }
            case PDF_CONTENT_OP_SET_FONT: {
                LOG_DIAG(
                    DEBUG,
                    RENDER,
                    "Set font: `%s` size %f",
                    op.data.set_font.font,
                    pdf_number_as_real(op.data.set_font.size)
                );

                RELEASE_ASSERT(resources->discriminant);
                RELEASE_ASSERT(resources->value.font.discriminant);

                PdfObject* font_object = pdf_dict_get(
                    &resources->value.font.value,
                    op.data.set_font.font
                );

                PDF_PROPAGATE(pdf_deserialize_font(
                    font_object,
                    arena,
                    resolver,
                    &state->graphics_state.text_state.text_font
                ));
                state->graphics_state.text_state.text_font_size =
                    pdf_number_as_real(op.data.set_font.size);

                break;
            }
            case PDF_CONTENT_OP_NEXT_LINE: {
                GeomMat3 transform = geom_mat3_new(
                    1.0,
                    0.0,
                    0.0,
                    0.0,
                    1.0,
                    0.0,
                    pdf_number_as_real(op.data.next_line.t_x),
                    pdf_number_as_real(op.data.next_line.t_y),
                    1.0
                );

                state->text_object_state.text_matrix = geom_mat3_mul(
                    transform,
                    state->text_object_state.text_line_matrix
                );
                state->text_object_state.text_line_matrix =
                    state->text_object_state.text_matrix;
                break;
            }
            case PDF_CONTENT_OP_SHOW_TEXT: {
                PDF_PROPAGATE(text_state_render(
                    arena,
                    canvas,
                    state->graphics_state.ctm,
                    &state->graphics_state.text_state,
                    &state->text_object_state,
                    op.data.show_text.text
                ));

                break;
            }
            case PDF_CONTENT_OP_SET_GRAY: {
                // TODO: colors
                break;
            }
            default: {
                LOG_TODO("Unimplemented content operation %d", op.kind);
            }
        }
    }

    return NULL;
}

PdfError* render_page(
    Arena* arena,
    PdfOptionalResolver resolver,
    const PdfPage* page,
    Canvas** canvas
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));
    RELEASE_ASSERT(page);
    RELEASE_ASSERT(canvas);
    RELEASE_ASSERT(!*canvas);

    GeomRect rect = geom_rect_new(
        geom_vec2_new(
            pdf_number_as_real(page->media_box.upper_right_x),
            pdf_number_as_real(page->media_box.upper_right_y)
        ),
        geom_vec2_new(
            pdf_number_as_real(page->media_box.lower_left_x),
            pdf_number_as_real(page->media_box.lower_left_y)
        )
    );

    RenderState state = {
        .graphics_state = graphics_state_default(),
        .text_object_state = text_object_state_default()
    };

    state.graphics_state.ctm = geom_mat3_mul(
        geom_mat3_translate(-rect.min.x * SCALE, -rect.min.y * SCALE),
        geom_mat3_new(
            SCALE,
            0.0,
            0.0,
            0.0,
            -SCALE,
            0.0,
            -rect.min.x * SCALE,
            rect.max.y * SCALE,
            1.0
        )
    );

    *canvas = canvas_new_scalable(
        arena,
        (uint32_t)geom_rect_size(rect).x,
        (uint32_t)geom_rect_size(rect).y,
        0xffffffff
    );

    if (page->contents.discriminant) {
        for (size_t contents_idx = 0;
             contents_idx < pdf_void_vec_len(page->contents.value.elements);
             contents_idx++) {
            void* content_ptr;
            RELEASE_ASSERT(pdf_void_vec_get(
                page->contents.value.elements,
                contents_idx,
                &content_ptr
            ));
            PdfStream* content = content_ptr;

            PdfContentStream stream;
            PDF_REQUIRE(pdf_deserialize_content_stream(content, arena, &stream)
            );

            PDF_PROPAGATE(process_content_stream(
                arena,
                &state,
                &stream,
                &page->resources,
                resolver,
                *canvas
            ));
        }
    }

    return NULL;
}
