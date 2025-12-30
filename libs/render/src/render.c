#include "render/render.h"

#include <stdint.h>
#include <stdio.h>

#include "arena/arena.h"
#include "cache.h"
#include "canvas/canvas.h"
#include "canvas/path_builder.h"
#include "geom/mat3.h"
#include "geom/rect.h"
#include "geom/vec2.h"
#include "graphics_state.h"
#include "logger/log.h"
#include "pdf/color_space.h"
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

static CanvasLineCap pdf_line_cap_to_canvas(PdfLineCapStyle line_cap) {
    switch (line_cap) {
        case PDF_LINE_CAP_STYLE_BUTT: {
            return CANVAS_LINECAP_BUTT;
        }
        case PDF_LINE_CAP_STYLE_ROUND: {
            return CANVAS_LINECAP_ROUND;
        }
        case PDF_LINE_CAP_STYLE_PROJECTING: {
            return CANVAS_LINECAP_SQUARE;
        }
    }

    LOG_PANIC("Unreachable");
}

static CanvasLineJoin pdf_line_join_to_canvas(PdfLineJoinStyle line_join) {
    switch (line_join) {
        case PDF_LINE_JOIN_STYLE_MITER: {
            return CANVAS_LINEJOIN_MITER;
        }
        case PDF_LINE_JOIN_STYLE_ROUND: {
            return CANVAS_LINEJOIN_ROUND;
        }
        case PDF_LINE_JOIN_STYLE_BEVEL: {
            return CANVAS_LINEJOIN_BEVEL;
        }
    }

    LOG_PANIC("Unreachable");
}

typedef struct {
    GraphicsStateStack* graphics_state_stack; // idx 0 == top
    TextObjectState text_object_state;
    PathBuilder* path;

    RenderCache cache;
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

static uint32_t pack_rgba(GeomVec3 rgb, double a) {
    return ((uint32_t)(rgb.x * 255.0) << 24) | ((uint32_t)(rgb.y * 255.0) << 16)
         | ((uint32_t)(rgb.z * 255.0) << 8) | (uint32_t)(a * 255.0);
}

static PdfError* process_content_stream(
    Arena* arena,
    RenderState* state,
    PdfContentStream* content_stream,
    const PdfResourcesOptional* resources,
    PdfResolver* resolver,
    Canvas* canvas
) {
    RELEASE_ASSERT(state);
    RELEASE_ASSERT(content_stream);
    RELEASE_ASSERT(resolver);
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
                RELEASE_ASSERT(
                    resources->has_value
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
                    resolver
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
                current_graphics_state(state)->ctm = geom_mat3_mul(
                    op.data.set_ctm,
                    current_graphics_state(state)->ctm
                );
                break;
            }
            case PDF_OPERATOR_m: {
                path_builder_new_contour(state->path, op.data.new_subpath);
                break;
            }
            case PDF_OPERATOR_l: {
                path_builder_line_to(state->path, op.data.line_to);
                break;
            }
            case PDF_OPERATOR_h: {
                path_builder_close_contour(state->path);
                break;
            }
            case PDF_OPERATOR_S: {
                CanvasBrush brush = {
                    .enable_fill = false,
                    .enable_stroke = true,
                    .stroke_rgba = pack_rgba(
                        current_graphics_state(state)->stroking_rgb,
                        current_graphics_state(state)->stroking_alpha
                    ),
                    .stroke_width = current_graphics_state(state)->line_width,
                    .line_cap = pdf_line_cap_to_canvas(
                        current_graphics_state(state)->line_cap
                    ),
                    .line_join = pdf_line_join_to_canvas(
                        current_graphics_state(state)->line_join
                    ),
                    .miter_limit = current_graphics_state(state)->miter_limit
                };

                path_builder_apply_transform(
                    state->path,
                    current_graphics_state(state)->ctm
                );
                canvas_draw_path(canvas, state->path, brush);
                state->path = path_builder_new(arena); // TODO: recycle
                break;
            }
            case PDF_OPERATOR_f: {
                CanvasBrush brush = {
                    .enable_fill = true,
                    .enable_stroke = false,
                    .fill_rgba = pack_rgba(
                        current_graphics_state(state)->nonstroking_rgb,
                        current_graphics_state(state)->nonstroking_alpha
                    )
                };

                path_builder_apply_transform(
                    state->path,
                    current_graphics_state(state)->ctm
                );
                canvas_draw_path(canvas, state->path, brush);
                state->path = path_builder_new(arena); // TODO: recycle
                break;
            }
            case PDF_OPERATOR_B: {
                CanvasBrush brush = {
                    .enable_fill = true,
                    .enable_stroke = true,
                    .fill_rgba = pack_rgba(
                        current_graphics_state(state)->nonstroking_rgb,
                        current_graphics_state(state)->nonstroking_alpha
                    ),
                    .stroke_rgba = pack_rgba(
                        current_graphics_state(state)->stroking_rgb,
                        current_graphics_state(state)->stroking_alpha
                    ),
                    .stroke_width = current_graphics_state(state)->line_width,
                    .line_cap = pdf_line_cap_to_canvas(
                        current_graphics_state(state)->line_cap
                    ),
                    .line_join = pdf_line_join_to_canvas(
                        current_graphics_state(state)->line_join
                    ),
                    .miter_limit = current_graphics_state(state)->miter_limit
                };

                path_builder_apply_transform(
                    state->path,
                    current_graphics_state(state)->ctm
                );
                canvas_draw_path(canvas, state->path, brush);
                state->path = path_builder_new(arena); // TODO: recycle
                break;
            }
            case PDF_OPERATOR_n: {
                state->path = path_builder_new(arena); // TODO: recycle
                break;
            }
            case PDF_OPERATOR_BT: {
                state->text_object_state = text_object_state_default();
                break;
            }
            case PDF_OPERATOR_ET: {
                break;
            }
            case PDF_OPERATOR_Tc: {
                current_graphics_state(state)->text_state.character_spacing =
                    op.data.set_text_metric;
                break;
            }
            case PDF_OPERATOR_Tw: {
                current_graphics_state(state)->text_state.word_spacing =
                    op.data.set_text_metric;
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

                RELEASE_ASSERT(
                    resources->has_value
                ); // TODO: This should be an error, and ideally a function
                RELEASE_ASSERT(resources->value.font.has_value);

                PdfObject* font_object = pdf_dict_get(
                    &resources->value.font.value,
                    op.data.set_font.font
                );

                PDF_PROPAGATE(pdf_deserialize_font(
                    font_object,
                    &current_graphics_state(state)->text_state.text_font,
                    resolver
                ));
                current_graphics_state(state)->text_state.text_font_size =
                    op.data.set_font.size;
                current_graphics_state(state)->text_state.font_set = true;
                break;
            }
            case PDF_OPERATOR_Td:
            case PDF_OPERATOR_TD: {
                if (op.kind == PDF_OPERATOR_TD) {
                    current_graphics_state(state)->text_state.leading =
                        -op.data.text_offset.y;
                }

                GeomMat3 transform = geom_mat3_translate(
                    op.data.text_offset.x,
                    op.data.text_offset.y
                );
                state->text_object_state.text_matrix = geom_mat3_mul(
                    transform,
                    state->text_object_state.text_matrix
                );
                break;
            }
            case PDF_OPERATOR_Tm: {
                state->text_object_state.text_matrix = op.data.set_text_matrix;
                state->text_object_state.text_line_matrix =
                    state->text_object_state.text_matrix;
                break;
            }
            case PDF_OPERATOR_T_star: {
                GeomMat3 transform = geom_mat3_translate(
                    0,
                    -current_graphics_state(state)->text_state.leading
                );
                state->text_object_state.text_matrix = geom_mat3_mul(
                    transform,
                    state->text_object_state.text_matrix
                );
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
                        TextState* text_state =
                            &current_graphics_state(state)->text_state;
                        double tx = -(element.value.offset / 1000.0)
                                  * text_state->text_font_size
                                  * text_state->horizontal_scaling;
                        LOG_DIAG(INFO, RENDER, "element offsetting: %f", tx);

                        GeomMat3 transform = geom_mat3_translate(
                            tx, // TODO: vertical
                            0.0
                        );
                        state->text_object_state.text_matrix = geom_mat3_mul(
                            transform,
                            state->text_object_state.text_matrix
                        );
                    } else {
                        // TODO: rendering mode
                        CanvasBrush brush = {
                            .enable_fill = true,
                            .enable_stroke = false,
                            .fill_rgba = pack_rgba(
                                current_graphics_state(state)->nonstroking_rgb,
                                current_graphics_state(state)->nonstroking_alpha
                            )
                        };

                        PDF_PROPAGATE(text_state_render(
                            arena,
                            canvas,
                            resolver,
                            &state->cache,
                            current_graphics_state(state)->ctm,
                            &current_graphics_state(state)->text_state,
                            &state->text_object_state,
                            element.value.str,
                            brush
                        ));
                    }
                }
                break;
            }
            case PDF_OPERATOR_cs: {
                RELEASE_ASSERT(resources->has_value);
                RELEASE_ASSERT(resources->value.color_space.has_value);

                PdfObject* color_space_object = pdf_dict_get(
                    &resources->value.color_space.value,
                    op.data.set_color_space
                );
                RELEASE_ASSERT(color_space_object);

                PDF_PROPAGATE(pdf_deserialize_color_space(
                    color_space_object,
                    &current_graphics_state(state)->nonstroking_color_space,
                    resolver
                ));

                break;
            }
            case PDF_OPERATOR_sc: {
                GraphicsState* graphics_state = current_graphics_state(state);

                switch (graphics_state->nonstroking_color_space.family) {
                    case PDF_COLOR_SPACE_CAL_RGB: {

                        if (pdf_object_vec_len(op.data.set_color) != 3) {
                            return PDF_ERROR(
                                PDF_ERR_MISSING_OPERAND,
                                "Expected 3 operands, found %zu",
                                pdf_object_vec_len(op.data.set_color)
                            );
                        }

                        PdfReal components[3];
                        for (size_t component_idx = 0; component_idx < 3;
                             component_idx++) {
                            PdfObject* component = NULL;
                            RELEASE_ASSERT(pdf_object_vec_get(
                                op.data.set_color,
                                component_idx,
                                &component
                            ));

                            PDF_PROPAGATE(pdf_deserialize_num_as_real(
                                component,
                                &components[component_idx],
                                resolver
                            ));
                        }

                        graphics_state->nonstroking_rgb = pdf_map_color(
                            components,
                            3,
                            graphics_state->nonstroking_color_space
                        );

                        break;
                    }
                    default: {
                        LOG_TODO(
                            "Unimplemented color space %d",
                            graphics_state->nonstroking_color_space.family
                        );
                    }
                }

                break;
            }
            case PDF_OPERATOR_G: {
                // TODO: default color spaces
                GraphicsState* graphics_state = current_graphics_state(state);
                graphics_state->stroking_color_space.family =
                    PDF_COLOR_SPACE_DEVICE_GRAY;

                PdfReal components[1] = {op.data.set_gray};
                graphics_state->stroking_rgb = pdf_map_color(
                    components,
                    1,
                    graphics_state->stroking_color_space
                );
                break;
            }
            case PDF_OPERATOR_RG: {
                // TODO: default color spaces
                GraphicsState* graphics_state = current_graphics_state(state);
                graphics_state->stroking_color_space.family =
                    PDF_COLOR_SPACE_DEVICE_RGB;

                PdfReal components[3] =
                    {op.data.set_rgb.r, op.data.set_rgb.g, op.data.set_rgb.b};
                graphics_state->stroking_rgb = pdf_map_color(
                    components,
                    3,
                    graphics_state->stroking_color_space
                );
                break;
            }
            case PDF_OPERATOR_rg: {
                // TODO: default color spaces
                GraphicsState* graphics_state = current_graphics_state(state);
                graphics_state->nonstroking_color_space.family =
                    PDF_COLOR_SPACE_DEVICE_RGB;

                PdfReal components[3] =
                    {op.data.set_rgb.r, op.data.set_rgb.g, op.data.set_rgb.b};
                graphics_state->nonstroking_rgb = pdf_map_color(
                    components,
                    3,
                    graphics_state->nonstroking_color_space
                );
                break;
            }
            case PDF_OPERATOR_K: {
                // TODO: default color spaces
                GraphicsState* graphics_state = current_graphics_state(state);
                graphics_state->stroking_color_space.family =
                    PDF_COLOR_SPACE_DEVICE_CMYK;

                PdfReal components[4] = {
                    op.data.set_cmyk.c,
                    op.data.set_cmyk.m,
                    op.data.set_cmyk.y,
                    op.data.set_cmyk.k
                };
                graphics_state->stroking_rgb = pdf_map_color(
                    components,
                    4,
                    graphics_state->stroking_color_space
                );

                break;
            }
            case PDF_OPERATOR_k: {
                // TODO: default color spaces
                GraphicsState* graphics_state = current_graphics_state(state);
                graphics_state->nonstroking_color_space.family =
                    PDF_COLOR_SPACE_DEVICE_CMYK;

                PdfReal components[4] = {
                    op.data.set_cmyk.c,
                    op.data.set_cmyk.m,
                    op.data.set_cmyk.y,
                    op.data.set_cmyk.k
                };
                graphics_state->nonstroking_rgb = pdf_map_color(
                    components,
                    4,
                    graphics_state->nonstroking_color_space
                );

                break;
            }
            case PDF_OPERATOR_Do: {
                RELEASE_ASSERT(resources->has_value);
                RELEASE_ASSERT(resources->value.xobject.has_value);

                PdfObject* xobject_object = pdf_dict_get(
                    &resources->value.xobject.value,
                    op.data.paint_xobject
                );
                RELEASE_ASSERT(xobject_object);

                PdfXObject xobject;
                PDF_PROPAGATE(
                    pdf_deserialize_xobject(xobject_object, &xobject, resolver)
                );

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
    PdfResolver* resolver,
    const PdfPage* page,
    Canvas** canvas
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(resolver);
    RELEASE_ASSERT(page);
    RELEASE_ASSERT(canvas);
    RELEASE_ASSERT(!*canvas);
    RELEASE_ASSERT(page->media_box.has_value);

    PdfRectangle mediabox = page->media_box.value;

    GeomRect rect = geom_rect_new(
        geom_vec2_new(
            pdf_number_as_real(mediabox.upper_right_x),
            pdf_number_as_real(mediabox.upper_right_y)
        ),
        geom_vec2_new(
            pdf_number_as_real(mediabox.lower_left_x),
            pdf_number_as_real(mediabox.lower_left_y)
        )
    );

    RenderState state = {
        .graphics_state_stack = graphics_state_stack_new(arena),
        .text_object_state = text_object_state_default(),
        .cache.cmap_cache = pdf_cmap_cache_new(arena),
        .cache.glyph_list = NULL,
        .path = path_builder_new(arena)
    };

    graphics_state_stack_push_back(
        state.graphics_state_stack,
        graphics_state_default()
    );

    double scale = 1.0; // TODO: load
    current_graphics_state(&state)->ctm = geom_mat3_mul(
        geom_mat3_translate(-rect.min.x * scale, -rect.min.y * scale),
        geom_mat3_new(
            scale,
            0.0,
            0.0,
            0.0,
            -scale,
            0.0,
            -rect.min.x * scale,
            rect.max.y * scale,
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

            PdfContentStream stream;
            PDF_PROPAGATE(
                pdf_resolve_content_stream(stream_ref, resolver, &stream)
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
