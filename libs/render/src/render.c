#include "render/render.h"

#include <stdio.h>

#include "arena/arena.h"
#include "canvas/canvas.h"
#include "geom/mat3.h"
#include "geom/rect.h"
#include "geom/vec2.h"
#include "graphics_state.h"
#include "logger/log.h"
#include "pdf/content_op.h"
#include "pdf/content_stream.h"
#include "pdf/fonts/cmap.h"
#include "pdf/fonts/font.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"
#include "text_state.h"

#define SCALE (25.4 / 72.0) // TODO: Load user-space units for conversion factor

typedef struct {
    GraphicsStateStack* graphics_state_stack; // idx 0 == top
    TextObjectState text_object_state;

    PdfCMapCache* cmap_cache;
} RenderState;

static GraphicsState* current_graphics_state(RenderState* state) {
    RELEASE_ASSERT(state);

    GraphicsState* gstate = NULL;
    RELEASE_ASSERT(
        graphics_state_stack_get_ptr(state->graphics_state_stack, 0, &gstate)
    );
    return gstate;
}

static void save_graphics_state(RenderState* state) {
    RELEASE_ASSERT(state);

    graphics_state_stack_push_front(
        state->graphics_state_stack,
        *current_graphics_state(state)
    );
}

static PdfError* restore_graphics_state(RenderState* state) {
    RELEASE_ASSERT(state);

    if (!graphics_state_stack_pop_front(state->graphics_state_stack, NULL)) {
        return PDF_ERROR(
            PDF_ERR_RENDER_GSTATE_CANNOT_RESTORE,
            "GState stack underflow"
        );
    }

    return NULL;
}

static PdfError* process_content_stream(
    Arena* arena,
    RenderState* state,
    PdfContentStream* content_stream,
    const PdfResourcesOptional* resources,
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
            case PDF_CONTENT_OP_SAVE_GSTATE: {
                save_graphics_state(state);
                break;
            }
            case PDF_CONTENT_OP_RESTORE_GSTATE: {
                PDF_PROPAGATE(restore_graphics_state(state));
                break;
            }
            case PDF_CONTENT_OP_SET_CTM: {
                current_graphics_state(state)->ctm = geom_mat3_new_pdf(
                    pdf_number_as_real(op.data.set_ctm.a),
                    pdf_number_as_real(op.data.set_ctm.b),
                    pdf_number_as_real(op.data.set_ctm.c),
                    pdf_number_as_real(op.data.set_ctm.d),
                    pdf_number_as_real(op.data.set_ctm.e),
                    pdf_number_as_real(op.data.set_ctm.f)
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

                RELEASE_ASSERT(resources->has_value
                ); // TODO: This should be an error, and ideally a function
                RELEASE_ASSERT(resources->value.font.has_value);

                PdfObject* font_object = pdf_dict_get(
                    &resources->value.font.value,
                    op.data.set_font.font
                );

                PDF_PROPAGATE(pdf_deserialize_font(
                    font_object,
                    &current_graphics_state(state)->text_state.text_font,
                    resolver,
                    arena
                ));
                current_graphics_state(state)->text_state.text_font_size =
                    pdf_number_as_real(op.data.set_font.size);
                current_graphics_state(state)->text_state.font_set = true;

                break;
            }
            // case PDF_CONTENT_OP_NEXT_LINE: {
            //     GeomMat3 transform = geom_mat3_new(
            //         1.0,
            //         0.0,
            //         0.0,
            //         0.0,
            //         1.0,
            //         0.0,
            //         pdf_number_as_real(op.data.next_line.t_x),
            //         pdf_number_as_real(op.data.next_line.t_y),
            //         1.0
            //     );

            //     state->text_object_state.text_matrix = geom_mat3_mul(
            //         transform,
            //         state->text_object_state.text_line_matrix
            //     );
            //     state->text_object_state.text_line_matrix =
            //         state->text_object_state.text_matrix;
            //     break;
            // }
            case PDF_CONTENT_OP_SET_TEXT_MATRIX: {
                state->text_object_state.text_matrix = geom_mat3_new_pdf(
                    pdf_number_as_real(op.data.set_text_matrix.a),
                    pdf_number_as_real(op.data.set_text_matrix.b),
                    pdf_number_as_real(op.data.set_text_matrix.c),
                    pdf_number_as_real(op.data.set_text_matrix.d),
                    pdf_number_as_real(op.data.set_text_matrix.e),
                    pdf_number_as_real(op.data.set_text_matrix.f)
                );
                state->text_object_state.text_line_matrix =
                    state->text_object_state.text_matrix;
                break;
            }
            case PDF_CONTENT_OP_SHOW_TEXT: {
                PDF_PROPAGATE(text_state_render(
                    arena,
                    canvas,
                    resolver,
                    state->cmap_cache,
                    current_graphics_state(state)->ctm,
                    &current_graphics_state(state)->text_state,
                    &state->text_object_state,
                    op.data.show_text
                ));

                break;
            }
            case PDF_CONTENT_OP_SET_GRAY: {
                // TODO: colors
                break;
            }
            // case PDF_CONTENT_OP_PAINT_XOBJECT: {
            //     RELEASE_ASSERT(resources->has_value);
            //     RELEASE_ASSERT(resources->value.xobject.has_value);

            //     PdfObject* xobject_object = pdf_dict_get(
            //         &resources->value.xobject.value,
            //         op.data.paint_xobject.xobject
            //     );

            //     PdfXObject xobject;
            //     PDF_PROPAGATE(pdf_deserialize_xobject(
            //         xobject_object,
            //         &xobject,
            //         resolver,
            //         arena
            //     ));

            //     switch (xobject.type) {
            //         case PDF_XOBJECT_IMAGE: {
            //             LOG_TODO();
            //         }
            //         case PDF_XOBJECT_FORM: {
            //             // PdfFormXObject* form = &xobject.data.form;

            //             save_graphics_state(state);
            //             // TODO: apply matrix, clip with bbox

            //             PDF_PROPAGATE(restore_graphics_state(state));
            //         }
            //     }

            //     break;
            // }
            default: {
                LOG_TODO("Unimplemented render content operation %d", op.kind);
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
        .graphics_state_stack = graphics_state_stack_new(arena),
        .text_object_state = text_object_state_default(),
        .cmap_cache = pdf_cmap_cache_new(arena)
    };

    graphics_state_stack_push_back(
        state.graphics_state_stack,
        graphics_state_default()
    );

    current_graphics_state(&state)->ctm = geom_mat3_mul(
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

    if (page->contents.has_value) {
        for (size_t contents_idx = 0;
             contents_idx
             < pdf_content_stream_ref_vec_len(page->contents.value);
             contents_idx++) {
            PdfContentStreamRef stream_ref;
            RELEASE_ASSERT(pdf_content_stream_ref_vec_get(
                page->contents.value,
                contents_idx,
                &stream_ref
            ));

            RELEASE_ASSERT(resolver.present);
            RELEASE_ASSERT(resolver.resolver);

            PdfContentStream stream;
            PDF_PROPAGATE(pdf_resolve_content_stream(
                stream_ref,
                resolver.resolver,
                &stream
            ));

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
