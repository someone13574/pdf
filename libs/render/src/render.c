#include "render/render.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "arena/arena.h"
#include "cache.h"
#include "canvas/canvas.h"
#include "canvas/path_builder.h"
#include "color/icc_cache.h"
#include "err/error.h"
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
#include "pdf/shading.h"
#include "pdf/types.h"
#include "pdf/xobject.h"
#include "shading.h"
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
    PathBuilderOptions path_options;
    PathBuilder* path;
    double stroke_width_scale;
    bool pending_clip;
    bool pending_clip_even_odd;

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

static Error* restore_graphics_state(RenderState* state, Canvas* canvas) {
    RELEASE_ASSERT(state);
    RELEASE_ASSERT(canvas);

    if (graphics_state_stack_len(state->graphics_state_stack) <= 1) {
        return ERROR(
            RENDER_ERR_GSTATE_CANNOT_RESTORE,
            "GState stack underflow"
        );
    }

    GraphicsState popped;
    RELEASE_ASSERT(
        graphics_state_stack_pop_front(state->graphics_state_stack, &popped)
    );

    GraphicsState* restored = current_graphics_state(state);
    RELEASE_ASSERT(popped.clip_depth >= restored->clip_depth);
    size_t clips_to_pop = popped.clip_depth - restored->clip_depth;
    if (clips_to_pop != 0) {
        canvas_pop_clip_paths(canvas, clips_to_pop);
    }

    return NULL;
}

static Rgba make_rgba(GeomVec3 rgb, double a) {
    return rgba_from_rgb(rgb_from_geom(rgb), a);
}

static double render_current_stroke_width(RenderState* state) {
    RELEASE_ASSERT(state);
    return current_graphics_state(state)->line_width * state->stroke_width_scale;
}

static double render_ctm_max_linear_scale(GeomMat3 ctm) {
    double a = ctm.mat[0][0];
    double b = ctm.mat[0][1];
    double c = ctm.mat[1][0];
    double d = ctm.mat[1][1];

    double trace = a * a + b * b + c * c + d * d;
    double det = a * d - b * c;
    double disc = trace * trace - 4.0 * det * det;
    if (disc < 0.0) {
        disc = 0.0;
    }

    double sigma_max = sqrt(0.5 * (trace + sqrt(disc)));
    if (sigma_max < 1e-9) {
        return 1e-9;
    }

    return sigma_max;
}

static PathBuilderOptions
render_current_path_options(RenderState* state, Canvas* canvas) {
    RELEASE_ASSERT(state);
    RELEASE_ASSERT(canvas);

    PathBuilderOptions options = state->path_options;
    if (!options.flatten_curves) {
        return options;
    }

    double flatness = current_graphics_state(state)->flatness;
    if (flatness <= 1e-9) {
        flatness = 1e-6;
    }

    double ctm_scale =
        render_ctm_max_linear_scale(current_graphics_state(state)->ctm);
    double raster_scale = 1.0;
    if (canvas_is_raster(canvas)) {
        raster_scale = 1.0 / canvas_raster_res(canvas);
    }

    double user_space_flatness = flatness / (ctm_scale * raster_scale);
    if (user_space_flatness <= 1e-9) {
        user_space_flatness = 1e-9;
    }

    options.quad_flatness = user_space_flatness;
    options.cubic_flatness = user_space_flatness;
    return options;
}

static void render_refresh_path_options(RenderState* state, Canvas* canvas) {
    RELEASE_ASSERT(state);
    RELEASE_ASSERT(canvas);

    if (!state->path) {
        return;
    }

    path_builder_set_options(
        state->path,
        render_current_path_options(state, canvas)
    );
}

static void
consume_current_path(Arena* arena, RenderState* state, Canvas* canvas) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(state);
    RELEASE_ASSERT(canvas);

    state->path = path_builder_new_with_options(
        arena,
        render_current_path_options(state, canvas)
    ); // TODO: recycle
}

static void apply_pending_clip_path(RenderState* state, Canvas* canvas) {
    RELEASE_ASSERT(state);
    RELEASE_ASSERT(canvas);

    if (!state->pending_clip) {
        return;
    }

    canvas_push_clip_path(canvas, state->path, state->pending_clip_even_odd);
    current_graphics_state(state)->clip_depth++;
    state->pending_clip = false;
    state->pending_clip_even_odd = false;
}

static Error* process_content_stream(
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
            case PDF_OPERATOR_i: {
                current_graphics_state(state)->flatness = op.data.flatness;
                render_refresh_path_options(state, canvas);
                break;
            }
            case PDF_OPERATOR_gs: {
                RELEASE_ASSERT(resources->is_some); // TODO: Make this an error
                RELEASE_ASSERT(resources->value.ext_gstate.is_some);

                PdfObject gstate_object;
                TRY(pdf_object_dict_get(
                    &resources->value.ext_gstate.value,
                    op.data.set_gstate,
                    &gstate_object
                ));

                PdfGStateParams params;
                TRY(
                    pdf_deserde_gstate_params(&gstate_object, &params, resolver)
                );

                graphics_state_apply_params(
                    current_graphics_state(state),
                    params
                );
                render_refresh_path_options(state, canvas);
                break;
            }
            case PDF_OPERATOR_q: {
                save_graphics_state(state);
                break;
            }
            case PDF_OPERATOR_Q: {
                TRY(restore_graphics_state(state, canvas));
                render_refresh_path_options(state, canvas);
                break;
            }
            case PDF_OPERATOR_cm: {
                current_graphics_state(state)->ctm = geom_mat3_mul(
                    op.data.set_ctm,
                    current_graphics_state(state)->ctm
                );
                render_refresh_path_options(state, canvas);
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
            case PDF_OPERATOR_c: {
                path_builder_cubic_bezier_to(
                    state->path,
                    op.data.cubic_bezier.end,
                    op.data.cubic_bezier.c1,
                    op.data.cubic_bezier.c2
                );
                break;
            }
            case PDF_OPERATOR_v: {
                path_builder_cubic_bezier_to(
                    state->path,
                    op.data.part_cubic_bezier.b,
                    path_builder_position(state->path),
                    op.data.part_cubic_bezier.a
                );
                break;
            }
            case PDF_OPERATOR_y: {
                path_builder_cubic_bezier_to(
                    state->path,
                    op.data.part_cubic_bezier.b,
                    path_builder_position(state->path),
                    op.data.part_cubic_bezier.b
                );
                break;
            }
            case PDF_OPERATOR_h: {
                path_builder_close_contour(state->path);
                break;
            }
            case PDF_OPERATOR_W: {
                state->pending_clip = true;
                state->pending_clip_even_odd = false;
                break;
            }
            case PDF_OPERATOR_W_star: {
                state->pending_clip = true;
                state->pending_clip_even_odd = true;
                break;
            }
            case PDF_OPERATOR_S: {
                CanvasBrush brush = {
                    .enable_fill = false,
                    .enable_stroke = true,
                    .stroke_rgba = make_rgba(
                        current_graphics_state(state)->stroking_rgb,
                        current_graphics_state(state)->stroking_alpha
                    ),
                    .stroke_width = render_current_stroke_width(state),
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
                apply_pending_clip_path(state, canvas);
                canvas_draw_path(canvas, state->path, brush);
                consume_current_path(arena, state, canvas);
                break;
            }
            case PDF_OPERATOR_F:
            case PDF_OPERATOR_f: {
                CanvasBrush brush = {
                    .enable_fill = true,
                    .enable_stroke = false,
                    .fill_rgba = make_rgba(
                        current_graphics_state(state)->nonstroking_rgb,
                        current_graphics_state(state)->nonstroking_alpha
                    )
                };

                path_builder_apply_transform(
                    state->path,
                    current_graphics_state(state)->ctm
                );
                apply_pending_clip_path(state, canvas);
                canvas_draw_path(canvas, state->path, brush);
                consume_current_path(arena, state, canvas);
                break;
            }
            case PDF_OPERATOR_f_star: {
                CanvasBrush brush = {
                    .enable_fill = true,
                    .even_odd_fill = true,
                    .enable_stroke = false,
                    .fill_rgba = make_rgba(
                        current_graphics_state(state)->nonstroking_rgb,
                        current_graphics_state(state)->nonstroking_alpha
                    )
                };

                path_builder_apply_transform(
                    state->path,
                    current_graphics_state(state)->ctm
                );
                apply_pending_clip_path(state, canvas);
                canvas_draw_path(canvas, state->path, brush);
                consume_current_path(arena, state, canvas);
                break;
            }
            case PDF_OPERATOR_B: {
                CanvasBrush brush = {
                    .enable_fill = true,
                    .enable_stroke = true,
                    .fill_rgba = make_rgba(
                        current_graphics_state(state)->nonstroking_rgb,
                        current_graphics_state(state)->nonstroking_alpha
                    ),
                    .stroke_rgba = make_rgba(
                        current_graphics_state(state)->stroking_rgb,
                        current_graphics_state(state)->stroking_alpha
                    ),
                    .stroke_width = render_current_stroke_width(state),
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
                apply_pending_clip_path(state, canvas);
                canvas_draw_path(canvas, state->path, brush);
                consume_current_path(arena, state, canvas);
                break;
            }
            case PDF_OPERATOR_B_star: {
                CanvasBrush brush = {
                    .enable_fill = true,
                    .even_odd_fill = true,
                    .enable_stroke = true,
                    .fill_rgba = make_rgba(
                        current_graphics_state(state)->nonstroking_rgb,
                        current_graphics_state(state)->nonstroking_alpha
                    ),
                    .stroke_rgba = make_rgba(
                        current_graphics_state(state)->stroking_rgb,
                        current_graphics_state(state)->stroking_alpha
                    ),
                    .stroke_width = render_current_stroke_width(state),
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
                apply_pending_clip_path(state, canvas);
                canvas_draw_path(canvas, state->path, brush);
                consume_current_path(arena, state, canvas);
                break;
            }
            case PDF_OPERATOR_b: {
                path_builder_close_contour(state->path);

                CanvasBrush brush = {
                    .enable_fill = true,
                    .enable_stroke = true,
                    .fill_rgba = make_rgba(
                        current_graphics_state(state)->nonstroking_rgb,
                        current_graphics_state(state)->nonstroking_alpha
                    ),
                    .stroke_rgba = make_rgba(
                        current_graphics_state(state)->stroking_rgb,
                        current_graphics_state(state)->stroking_alpha
                    ),
                    .stroke_width = render_current_stroke_width(state),
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
                apply_pending_clip_path(state, canvas);
                canvas_draw_path(canvas, state->path, brush);
                consume_current_path(arena, state, canvas);
                break;
            }
            case PDF_OPERATOR_b_star: {
                path_builder_close_contour(state->path);

                CanvasBrush brush = {
                    .enable_fill = true,
                    .even_odd_fill = true,
                    .enable_stroke = true,
                    .fill_rgba = make_rgba(
                        current_graphics_state(state)->nonstroking_rgb,
                        current_graphics_state(state)->nonstroking_alpha
                    ),
                    .stroke_rgba = make_rgba(
                        current_graphics_state(state)->stroking_rgb,
                        current_graphics_state(state)->stroking_alpha
                    ),
                    .stroke_width = render_current_stroke_width(state),
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
                apply_pending_clip_path(state, canvas);
                canvas_draw_path(canvas, state->path, brush);
                consume_current_path(arena, state, canvas);
                break;
            }
            case PDF_OPERATOR_n: {
                if (state->pending_clip) {
                    path_builder_apply_transform(
                        state->path,
                        current_graphics_state(state)->ctm
                    );
                    apply_pending_clip_path(state, canvas);
                }
                consume_current_path(arena, state, canvas);
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
            case PDF_OPERATOR_TL: {
                current_graphics_state(state)->text_state.leading =
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
                    resources->is_some
                ); // TODO: This should be an error, and ideally a function
                RELEASE_ASSERT(resources->value.font.is_some);

                PdfObject font_object;
                TRY(pdf_object_dict_get(
                    &resources->value.font.value,
                    op.data.set_font.font,
                    &font_object
                ));

                TRY(pdf_deserde_font(
                    &font_object,
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
                state->text_object_state.text_line_matrix = geom_mat3_mul(
                    transform,
                    state->text_object_state.text_line_matrix
                );
                state->text_object_state.text_matrix =
                    state->text_object_state.text_line_matrix;
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
                            .fill_rgba = make_rgba(
                                current_graphics_state(state)->nonstroking_rgb,
                                current_graphics_state(state)->nonstroking_alpha
                            )
                        };

                        TRY(text_state_render(
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
                RELEASE_ASSERT(resources->is_some);
                RELEASE_ASSERT(resources->value.color_space.is_some);

                GraphicsState* graphics_state = current_graphics_state(state);

                PdfObject color_space_object;
                TRY(pdf_object_dict_get(
                    &resources->value.color_space.value,
                    op.data.set_color_space,
                    &color_space_object
                ));

                TRY(pdf_deserde_color_space(
                    &color_space_object,
                    &graphics_state->nonstroking_color_space,
                    resolver
                ));

                PdfReal components[4] = {0.0, 0.0, 0.0, 0.0};
                size_t num_components = 0;

                switch (graphics_state->nonstroking_color_space.family) {
                    case PDF_COLOR_SPACE_DEVICE_GRAY: {
                        num_components = 1;
                        break;
                    }
                    case PDF_COLOR_SPACE_DEVICE_RGB: {
                        num_components = 3;
                        break;
                    }
                    case PDF_COLOR_SPACE_DEVICE_CMYK: {
                        num_components = 4;
                        components[3] = 1.0;
                        break;
                    }
                    case PDF_COLOR_SPACE_CAL_GRAY: {
                        num_components = 1;
                        break;
                    }
                    case PDF_COLOR_SPACE_CAL_RGB: {
                        num_components = 3;
                        break;
                    }
                    case PDF_COLOR_SPACE_LAB: {
                        num_components = 4;
                        break;
                    }
                    case PDF_COLOR_SPACE_ICC_BASED: {
                        num_components = 0;
                        break;
                    }
                    default: {
                        LOG_TODO();
                        break;
                    }
                }

                if (num_components != 0) {
                    TRY(pdf_map_color(
                        components,
                        num_components,
                        graphics_state->nonstroking_color_space,
                        &state->cache.icc_cache,
                        &graphics_state->nonstroking_rgb
                    ));
                }

                break;
            }
            case PDF_OPERATOR_sc: {
                GraphicsState* graphics_state = current_graphics_state(state);

                switch (graphics_state->nonstroking_color_space.family) {
                    case PDF_COLOR_SPACE_CAL_RGB: {
                        if (pdf_object_vec_len(op.data.set_color) != 3) {
                            return ERROR(
                                PDF_ERR_MISSING_OPERAND,
                                "Expected 3 operands, found %zu",
                                pdf_object_vec_len(op.data.set_color)
                            );
                        }

                        PdfReal components[3];
                        for (size_t component_idx = 0; component_idx < 3;
                             component_idx++) {
                            PdfObject component;
                            RELEASE_ASSERT(pdf_object_vec_get(
                                op.data.set_color,
                                component_idx,
                                &component
                            ));

                            TRY(pdf_deserde_num_as_real(
                                &component,
                                &components[component_idx],
                                resolver
                            ));
                        }

                        TRY(pdf_map_color(
                            components,
                            3,
                            graphics_state->nonstroking_color_space,
                            &state->cache.icc_cache,
                            &graphics_state->nonstroking_rgb
                        ));

                        break;
                    }
                    case PDF_COLOR_SPACE_ICC_BASED: {
                        // Technically getting here is disallowed per the spec,
                        // but some files don't care
                        LOG_WARN(
                            RENDER,
                            "TODO: ICC Color spaces, %zu",
                            pdf_object_vec_len(op.data.set_color)
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
                TRY(pdf_map_color(
                    components,
                    1,
                    graphics_state->stroking_color_space,
                    &state->cache.icc_cache,
                    &graphics_state->stroking_rgb
                ));
                break;
            }
            case PDF_OPERATOR_g: {
                // TODO: default color spaces
                GraphicsState* graphics_state = current_graphics_state(state);
                graphics_state->nonstroking_color_space.family =
                    PDF_COLOR_SPACE_DEVICE_GRAY;

                PdfReal components[1] = {op.data.set_gray};
                TRY(pdf_map_color(
                    components,
                    1,
                    graphics_state->nonstroking_color_space,
                    &state->cache.icc_cache,
                    &graphics_state->nonstroking_rgb
                ));
                break;
            }
            case PDF_OPERATOR_RG: {
                // TODO: default color spaces
                GraphicsState* graphics_state = current_graphics_state(state);
                graphics_state->stroking_color_space.family =
                    PDF_COLOR_SPACE_DEVICE_RGB;

                PdfReal components[3] =
                    {op.data.set_rgb.r, op.data.set_rgb.g, op.data.set_rgb.b};
                TRY(pdf_map_color(
                    components,
                    3,
                    graphics_state->stroking_color_space,
                    &state->cache.icc_cache,
                    &graphics_state->stroking_rgb
                ));
                break;
            }
            case PDF_OPERATOR_rg: {
                // TODO: default color spaces
                GraphicsState* graphics_state = current_graphics_state(state);
                graphics_state->nonstroking_color_space.family =
                    PDF_COLOR_SPACE_DEVICE_RGB;

                PdfReal components[3] =
                    {op.data.set_rgb.r, op.data.set_rgb.g, op.data.set_rgb.b};
                TRY(pdf_map_color(
                    components,
                    3,
                    graphics_state->nonstroking_color_space,
                    &state->cache.icc_cache,
                    &graphics_state->nonstroking_rgb
                ));
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
                TRY(pdf_map_color(
                    components,
                    4,
                    graphics_state->stroking_color_space,
                    &state->cache.icc_cache,
                    &graphics_state->stroking_rgb
                ));

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
                TRY(pdf_map_color(
                    components,
                    4,
                    graphics_state->nonstroking_color_space,
                    &state->cache.icc_cache,
                    &graphics_state->nonstroking_rgb
                ));

                break;
            }
            case PDF_OPERATOR_sh: {
                RELEASE_ASSERT(resources->is_some);
                RELEASE_ASSERT(resources->value.shading.is_some);

                PdfObject shading_object;
                TRY(pdf_object_dict_get(
                    &resources->value.shading.value,
                    op.data.paint_shading,
                    &shading_object
                ));

                PdfObject resolved;
                TRY(pdf_resolve_object(
                    resolver,
                    &shading_object,
                    &resolved,
                    true
                ));

                PdfShadingDict shading_dict;
                TRY(
                    pdf_deserde_shading_dict(&resolved, &shading_dict, resolver)
                );

                render_shading(
                    &shading_dict,
                    arena,
                    current_graphics_state(state)->ctm,
                    canvas
                );

                break;
            }
            case PDF_OPERATOR_Do: {
                RELEASE_ASSERT(resources->is_some);
                RELEASE_ASSERT(resources->value.xobject.is_some);

                PdfObject xobject_object;
                TRY(pdf_object_dict_get(
                    &resources->value.xobject.value,
                    op.data.paint_xobject,
                    &xobject_object
                ));

                PdfXObject xobject;
                TRY(pdf_deserde_xobject(&xobject_object, &xobject, resolver));

                switch (xobject.type) {
                    case PDF_XOBJECT_IMAGE: {
                        LOG_TODO();
                    }
                    case PDF_XOBJECT_FORM: {
                        PdfFormXObject* form = &xobject.data.form;
                        GeomMat3 matrix;
                        if (form->matrix.is_some) {
                            matrix = form->matrix.value;
                        } else {
                            matrix = geom_mat3_identity();
                        }

                        save_graphics_state(state);
                        current_graphics_state(state)->ctm = geom_mat3_mul(
                            matrix,
                            current_graphics_state(state)->ctm
                        );

                        PathBuilder* clip_path = path_builder_new_with_options(
                            arena,
                            state->path_options
                        );
                        path_builder_new_contour(
                            clip_path,
                            geom_vec2_new(
                                pdf_number_as_real(form->bbox.lower_left_x),
                                pdf_number_as_real(form->bbox.lower_left_y)
                            )
                        );
                        path_builder_line_to(
                            clip_path,
                            geom_vec2_new(
                                pdf_number_as_real(form->bbox.upper_right_x),
                                pdf_number_as_real(form->bbox.lower_left_y)
                            )
                        );
                        path_builder_line_to(
                            clip_path,
                            geom_vec2_new(
                                pdf_number_as_real(form->bbox.upper_right_x),
                                pdf_number_as_real(form->bbox.upper_right_y)
                            )
                        );
                        path_builder_line_to(
                            clip_path,
                            geom_vec2_new(
                                pdf_number_as_real(form->bbox.lower_left_x),
                                pdf_number_as_real(form->bbox.upper_right_y)
                            )
                        );

                        path_builder_apply_transform(
                            clip_path,
                            current_graphics_state(state)->ctm
                        );
                        canvas_push_clip_path(canvas, clip_path, false);
                        current_graphics_state(state)->clip_depth++;

                        TRY(process_content_stream(
                            arena,
                            state,
                            &form->content_stream,
                            &form->resources,
                            resolver,
                            canvas
                        ));

                        TRY(restore_graphics_state(state, canvas));
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

Error* render_page(
    Arena* arena,
    PdfResolver* resolver,
    const PdfPage* page,
    RenderCanvasType canvas_type,
    Canvas** canvas
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(resolver);
    RELEASE_ASSERT(page);
    RELEASE_ASSERT(canvas);
    RELEASE_ASSERT(!*canvas);
    RELEASE_ASSERT(page->media_box.is_some);

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

    GeomVec2 rect_size = geom_rect_size(rect);
    uint32_t canvas_width = (uint32_t)ceil(rect_size.x);
    uint32_t canvas_height = (uint32_t)ceil(rect_size.y);
    double canvas_scale = 1.0;
    switch (canvas_type) {
        case RENDER_CANVAS_TYPE_RASTER: {
            const uint32_t resolution_multiplier = 5;
            canvas_scale = (double)resolution_multiplier;
            uint32_t raster_canvas_width =
                (uint32_t)ceil(rect_size.x * (double)resolution_multiplier);
            uint32_t raster_canvas_height =
                (uint32_t)ceil(rect_size.y * (double)resolution_multiplier);
            *canvas = canvas_new_raster(
                arena,
                raster_canvas_width,
                raster_canvas_height,
                rgba_new(1.0, 1.0, 1.0, 1.0)
            );
            break;
        }
        case RENDER_CANVAS_TYPE_SCALABLE: {
            *canvas = canvas_new_scalable(
                arena,
                canvas_width,
                canvas_height,
                rgba_new(1.0, 1.0, 1.0, 1.0),
                1.0
            );
            break;
        }
    }

    PathBuilderOptions path_options = path_builder_options_default();
    if (canvas_is_raster(*canvas)) {
        path_options = path_builder_options_flattened();
    }

    RenderState state = {
        .graphics_state_stack = graphics_state_stack_new(arena),
        .text_object_state = text_object_state_default(),
        .path_options = path_options,
        .stroke_width_scale = canvas_scale,
        .pending_clip = false,
        .pending_clip_even_odd = false,
        .cache.cmap_cache = pdf_cmap_cache_new(arena),
        .cache.glyph_list = NULL,
        .cache.icc_cache = icc_profile_cache_new(arena),
        .path = NULL
    };

    graphics_state_stack_push_back(
        state.graphics_state_stack,
        graphics_state_default()
    );

    double scale = canvas_scale; // TODO: load
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
    consume_current_path(arena, &state, *canvas);

    if (page->contents.is_some) {
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

            TRY(pdf_resolve_content_stream(&stream_ref, resolver));
            PdfContentStream* stream = stream_ref.resolved;

            TRY(process_content_stream(
                arena,
                &state,
                stream,
                &page->resources,
                resolver,
                *canvas
            ));
        }
    }

    return NULL;
}
