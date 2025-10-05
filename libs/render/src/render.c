#include "render/render.h"

#include <stdio.h>

#include "arena/arena.h"
#include "canvas/canvas.h"
#include "geom/mat3.h"
#include "geom/rect.h"
#include "geom/vec2.h"
#include "graphics_state.h"
#include "logger/log.h"
#include "pdf/content_stream/operation.h"
#include "pdf/content_stream/operator.h"
#include "pdf/fonts/cmap.h"
#include "pdf/fonts/font.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf/resources.h"
#include "pdf/xobject.h"
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
            case PDF_OPERATOR_w: {
                current_graphics_state(state)->line_width =
                    op.data.set_line_width;
                break;
            }
            case PDF_OPERATOR_J: {
                current_graphics_state(state)->line_join =
                    op.data.set_join_style;
                break;
            }
            case PDF_OPERATOR_M: {
                current_graphics_state(state)->miter_limit =
                    op.data.miter_limit;
                break;
            }
            case PDF_OPERATOR_gs: {
                RELEASE_ASSERT(resources->has_value
                ); // TODO: Make this an error
                RELEASE_ASSERT(resources->value.ext_gstate.has_value);

                PdfObject* gstate_object = pdf_dict_get(
                    &resources->value.ext_gstate.value,
                    op.data.set_gstate
                );
                RELEASE_ASSERT(gstate_object);

                PdfGStateParams params;
                PDF_PROPAGATE(pdf_deserialize_gstate_params(
                    gstate_object,
                    &params,
                    resolver,
                    arena
                ));

                graphics_state_apply_params(
                    current_graphics_state(state),
                    params
                );
                break;
            }
            case PDF_OPERATOR_q: {
                save_graphics_state(state);
                break;
            }
            case PDF_OPERATOR_Q: {
                PDF_PROPAGATE(restore_graphics_state(state));
                break;
            }
            case PDF_OPERATOR_cm: {
                current_graphics_state(state)->ctm = op.data.set_ctm;
                break;
            }
            case PDF_OPERATOR_m: {
                LOG_WARN(RENDER, "TODO: Paths");
                break;
            }
            case PDF_OPERATOR_l: {
                LOG_WARN(RENDER, "TODO: Paths");
                break;
            }
            case PDF_OPERATOR_h: {
                LOG_WARN(RENDER, "TODO: Paths");
                break;
            }
            case PDF_OPERATOR_f: {
                LOG_WARN(RENDER, "TODO: Paths");
                break;
            }
            case PDF_OPERATOR_B: {
                LOG_WARN(RENDER, "TODO: Paths");
                break;
            }
            case PDF_OPERATOR_BT: {
                state->text_object_state = text_object_state_default();
                break;
            }
            case PDF_OPERATOR_ET: {
                break;
            }
            case PDF_OPERATOR_Tf: {
                LOG_DIAG(
                    DEBUG,
                    RENDER,
                    "Set font: `%s` size %f",
                    op.data.set_font.font,
                    op.data.set_font.size
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
                    op.data.set_font.size;
                current_graphics_state(state)->text_state.font_set = true;
                break;
            }
            // // case PDF_CONTENT_OP_NEXT_LINE: {
            // //     GeomMat3 transform = geom_mat3_new(
            // //         1.0,
            // //         0.0,
            // //         0.0,
            // //         0.0,
            // //         1.0,
            // //         0.0,
            // //         pdf_number_as_real(op.data.next_line.t_x),
            // //         pdf_number_as_real(op.data.next_line.t_y),
            // //         1.0
            // //     );

            // //     state->text_object_state.text_matrix = geom_mat3_mul(
            // //         transform,
            // //         state->text_object_state.text_line_matrix
            // //     );
            // //     state->text_object_state.text_line_matrix =
            // //         state->text_object_state.text_matrix;
            // //     break;
            // // }
            case PDF_OPERATOR_Tm: {
                state->text_object_state.text_matrix = op.data.set_text_matrix;
                state->text_object_state.text_line_matrix =
                    state->text_object_state.text_matrix;
                break;
            }
            case PDF_OPERATOR_TJ: {
                for (size_t text_vec_idx = 0;
                     text_vec_idx < pdf_op_params_positioned_text_vec_len(
                         op.data.positioned_text
                     );
                     text_vec_idx++) {
                    PdfOpParamsPositionedTextElement
                        element; // TODO: This this terrible name
                    RELEASE_ASSERT(pdf_op_params_positioned_text_vec_get(
                        op.data.positioned_text,
                        text_vec_idx,
                        &element
                    ));

                    if (element.type == POSITIONED_TEXT_ELEMENT_OFFSET) {
                        LOG_TODO();
                    } else {
                        PDF_PROPAGATE(text_state_render(
                            arena,
                            canvas,
                            resolver,
                            state->cmap_cache,
                            current_graphics_state(state)->ctm,
                            &current_graphics_state(state)->text_state,
                            &state->text_object_state,
                            element.value.str
                        ));
                    }
                }
                break;
            }
            case PDF_OPERATOR_G:
            case PDF_OPERATOR_RG: {
                // TODO: Colors
                LOG_WARN(RENDER, "Colors not implemented");
                break;
            }
            case PDF_OPERATOR_rg: {
                // TODO: Colors
                LOG_WARN(RENDER, "Colors not implemented");
                break;
            }
            case PDF_OPERATOR_Do: {
                RELEASE_ASSERT(resources->has_value);
                RELEASE_ASSERT(resources->value.xobject.has_value);

                PdfObject* xobject_object = pdf_dict_get(
                    &resources->value.xobject.value,
                    op.data.paint_xobject
                );

                PdfXObject xobject;
                PDF_PROPAGATE(pdf_deserialize_xobject(
                    xobject_object,
                    &xobject,
                    resolver,
                    arena
                ));

                switch (xobject.type) {
                    case PDF_XOBJECT_IMAGE: {
                        LOG_TODO();
                    }
                    case PDF_XOBJECT_FORM: {
                        PdfFormXObject* form = &xobject.data.form;

                        save_graphics_state(state);
                        geom_mat3_mul(
                            current_graphics_state(state)->ctm,
                            form->matrix
                        );
                        // TODO: clip with bbox

                        PDF_PROPAGATE(process_content_stream(
                            arena,
                            state,
                            &form->content_stream,
                            &form->resources,
                            resolver,
                            canvas
                        ));

                        PDF_PROPAGATE(restore_graphics_state(state));
                    }
                }

                break;
            }
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
