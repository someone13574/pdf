#include "glyph.h"

#include <stddef.h>
#include <stdint.h>

#include "arena/arena.h"
#include "canvas/canvas.h"
#include "canvas/path_builder.h"
#include "err/error.h"
#include "geom/vec2.h"
#include "logger/log.h"
#include "parse_ctx/ctx.h"
#include "sfnt/glyph.h"
#include "sfnt/types.h"

#define DVEC_NAME SfntSimpleGlyphFlagsVec
#define DVEC_LOWERCASE_NAME sfnt_simple_glyph_flags_vec
#define DVEC_TYPE SfntSimpleGlyphFlags
#include "arena/dvec_impl.h"

#define DARRAY_NAME SfntGlyphPointArray
#define DARRAY_LOWERCASE_NAME sfnt_glyph_point_array
#define DARRAY_TYPE SfntGlyphPoint
#include "arena/darray_impl.h"

Error* sfnt_parse_simple_glyph(Arena* arena, ParseCtx ctx, SfntGlyph* glyph) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(glyph);
    RELEASE_ASSERT(glyph->num_contours > 0);

    SfntSimpleGlyph* data = &glyph->data.simple;

    TRY(parse_ctx_new_subctx(
        &ctx,
        (size_t)glyph->num_contours * 2,
        &data->end_pts_of_contours
    ));

    TRY(parse_ctx_read_u16_be(&ctx, &data->instruction_len));
    TRY(parse_ctx_new_subctx(&ctx, data->instruction_len, &data->instructions));

    uint16_t last_point_idx;
    REQUIRE(parse_ctx_get_u16_be(
        data->end_pts_of_contours,
        (size_t)glyph->num_contours - 1,
        &last_point_idx
    ));

    // Parse flags
    uint16_t point_idx = 0;
    data->flags = sfnt_simple_glyph_flags_vec_new(arena);

    while (point_idx <= last_point_idx) {
        SfntSimpleGlyphFlags flags = {.flags = 0, .repetitions = 0};
        TRY(parse_ctx_read_u8(&ctx, &flags.flags));

        if ((flags.flags & SFNT_SIMPLE_GLYPH_REPEAT) != 0) {
            TRY(parse_ctx_read_u8(&ctx, &flags.repetitions));
        }

        point_idx += (uint16_t)(flags.repetitions + 1);
        sfnt_simple_glyph_flags_vec_push(data->flags, flags);
    }

    data->points = sfnt_glyph_point_array_new(arena, last_point_idx + 1);

    // Parse x-coords
    size_t x_coord_idx = 0;
    for (size_t flag_idx = 0;
         flag_idx < sfnt_simple_glyph_flags_vec_len(data->flags);
         flag_idx++) {
        SfntSimpleGlyphFlags flags;
        RELEASE_ASSERT(
            sfnt_simple_glyph_flags_vec_get(data->flags, flag_idx, &flags)
        );
        bool on_curve = (flags.flags & SFNT_SIMPLE_GLYPH_ON_CURVE) != 0;

        // Coordinate stays the same
        if ((flags.flags & SFNT_SIMPLE_GLYPH_X_SHORT) == 0
            && (flags.flags & SFNT_SIMPLE_GLYPH_X_MODIFIER) != 0) {
            LOG_DIAG(TRACE, SFNT, "Delta-x (zero): %d", 0);
            for (size_t repetition = 0;
                 repetition < (size_t)(flags.repetitions + 1);
                 repetition++) {
                sfnt_glyph_point_array_set(
                    data->points,
                    x_coord_idx++,
                    (SfntGlyphPoint) {.on_curve = on_curve, .delta_x = 0}
                );
            }
            continue;
        }

        // Repeat
        for (size_t repetition = 0;
             repetition < (size_t)(flags.repetitions + 1);
             repetition++) {
            if ((flags.flags & SFNT_SIMPLE_GLYPH_X_SHORT) != 0) {
                bool positive =
                    (flags.flags & SFNT_SIMPLE_GLYPH_X_MODIFIER) != 0;
                uint8_t delta_mag;
                TRY(parse_ctx_read_u8(&ctx, &delta_mag));

                int16_t delta_x =
                    (int16_t)(positive ? delta_mag : -(int16_t)delta_mag);
                LOG_DIAG(TRACE, SFNT, "Delta-x (short): %d", delta_x);
                sfnt_glyph_point_array_set(
                    data->points,
                    x_coord_idx++,
                    (SfntGlyphPoint) {.on_curve = on_curve, .delta_x = delta_x}
                );
            } else {
                int16_t delta_x;
                TRY(parse_ctx_read_i16_be(&ctx, &delta_x));

                LOG_DIAG(TRACE, SFNT, "Delta-x: %d", delta_x);
                sfnt_glyph_point_array_set(
                    data->points,
                    x_coord_idx++,
                    (SfntGlyphPoint) {.on_curve = on_curve, .delta_x = delta_x}
                );
            }
        }
    }

    // Parse y-coords
    size_t y_coord_idx = 0;
    for (size_t flag_idx = 0;
         flag_idx < sfnt_simple_glyph_flags_vec_len(data->flags);
         flag_idx++) {
        SfntSimpleGlyphFlags flags;
        RELEASE_ASSERT(
            sfnt_simple_glyph_flags_vec_get(data->flags, flag_idx, &flags)
        );

        // Coordinate stays the same
        if ((flags.flags & SFNT_SIMPLE_GLYPH_Y_SHORT) == 0
            && (flags.flags & SFNT_SIMPLE_GLYPH_Y_MODIFIER) != 0) {
            LOG_DIAG(TRACE, SFNT, "Delta-y (zero): %d", 0);
            for (size_t repetition = 0;
                 repetition < (size_t)(flags.repetitions + 1);
                 repetition++) {
                SfntGlyphPoint* point;
                RELEASE_ASSERT(sfnt_glyph_point_array_get_ptr(
                    data->points,
                    y_coord_idx++,
                    &point
                ));
                point->delta_y = 0;
            }

            continue;
        }

        // Repeat
        for (size_t repetition = 0;
             repetition < (size_t)(flags.repetitions + 1);
             repetition++) {
            if ((flags.flags & SFNT_SIMPLE_GLYPH_Y_SHORT) != 0) {
                bool positive =
                    (flags.flags & SFNT_SIMPLE_GLYPH_Y_MODIFIER) != 0;
                uint8_t delta_mag;
                TRY(parse_ctx_read_u8(&ctx, &delta_mag));

                int16_t delta_y =
                    (int16_t)(positive ? delta_mag : -(int16_t)delta_mag);
                LOG_DIAG(TRACE, SFNT, "Delta-y (short): %d", delta_y);
                SfntGlyphPoint* point;
                RELEASE_ASSERT(sfnt_glyph_point_array_get_ptr(
                    data->points,
                    y_coord_idx++,
                    &point
                ));
                point->delta_y = delta_y;
            } else {
                int16_t delta_y;
                TRY(parse_ctx_read_i16_be(&ctx, &delta_y));

                LOG_DIAG(TRACE, SFNT, "Delta-y: %d", delta_y);
                SfntGlyphPoint* point;
                RELEASE_ASSERT(sfnt_glyph_point_array_get_ptr(
                    data->points,
                    y_coord_idx++,
                    &point
                ));
                point->delta_y = delta_y;
            }
        }
    }

    return NULL;
}

Error* sfnt_parse_glyph(Arena* arena, ParseCtx ctx, SfntGlyph* glyph) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(glyph);

    TRY(parse_ctx_read_i16_be(&ctx, &glyph->num_contours));
    TRY(sfnt_read_fword(&ctx, &glyph->x_min));
    TRY(sfnt_read_fword(&ctx, &glyph->y_min));
    TRY(sfnt_read_fword(&ctx, &glyph->x_max));
    TRY(sfnt_read_fword(&ctx, &glyph->y_max));

    if (glyph->num_contours == 0) {
        glyph->glyph_type = SFNT_GLYPH_TYPE_NONE;
        return NULL;
    }

    if (glyph->num_contours > 0) {
        glyph->glyph_type = SFNT_GLYPH_TYPE_SIMPLE;
        return sfnt_parse_simple_glyph(arena, ctx, glyph);
    }

    LOG_TODO("Compound glyphs");
}

// TODO: Fully translate this to using the geometry helper lib
void sfnt_glyph_render(
    Canvas* canvas,
    const SfntGlyph* glyph,
    GeomMat3 transform,
    CanvasBrush brush
) {
    RELEASE_ASSERT(canvas);
    RELEASE_ASSERT(glyph);

    if (glyph->num_contours == 0) {
        return;
    }

    Arena* temp_arena = arena_new(4096);
    PathBuilder* path = path_builder_new(temp_arena);

    if (glyph->glyph_type != SFNT_GLYPH_TYPE_SIMPLE) {
        LOG_TODO("Only simple glyphs are supported");
    }

    SfntSimpleGlyph data = glyph->data.simple;

    int32_t x_coord = 0;
    int32_t y_coord = 0;

    for (int16_t contour_idx = 0; contour_idx < glyph->num_contours;
         contour_idx++) {
        uint16_t contour_end;
        REQUIRE(parse_ctx_get_u16_be(
            data.end_pts_of_contours,
            (size_t)contour_idx,
            &contour_end
        ));

        uint16_t contour_start = 0;
        if (contour_idx != 0) {
            REQUIRE(parse_ctx_get_u16_be(
                data.end_pts_of_contours,
                (size_t)contour_idx - 1,
                &contour_start
            ));
            contour_start++;
        }

        bool has_curve_start = false;
        bool has_curve_control = false;

        int32_t contour_start_x = x_coord;
        int32_t contour_start_y = y_coord;
        int32_t contour_end_x = x_coord;
        int32_t contour_end_y = y_coord;
        GeomVec2 curve_control = geom_vec2_new(0.0, 0.0);

        for (size_t pass = 0; pass < 2; pass++) {
            x_coord = contour_start_x;
            y_coord = contour_start_y;

            for (size_t point_idx = contour_start; point_idx <= contour_end;
                 point_idx++) {
                SfntGlyphPoint point;
                RELEASE_ASSERT(
                    sfnt_glyph_point_array_get(data.points, point_idx, &point)
                );

                x_coord += point.delta_x;
                y_coord += point.delta_y;

                if (point_idx == contour_end) {
                    contour_end_x = x_coord;
                    contour_end_y = y_coord;
                }

                GeomVec2 position = geom_vec2_transform(
                    geom_vec2_new((double)x_coord, (double)y_coord),
                    transform
                );

                if (!has_curve_start && point.on_curve) {
                    has_curve_start = true;
                    path_builder_new_contour(path, position);
                } else if (has_curve_start && !has_curve_control
                           && !point.on_curve) {
                    has_curve_control = true;
                    curve_control = position;
                } else if (has_curve_start && !has_curve_control
                           && point.on_curve) {
                    path_builder_line_to(path, position);

                    if (pass != 0) {
                        break;
                    }
                } else if (has_curve_start && has_curve_control
                           && point.on_curve) {
                    path_builder_quad_bezier_to(path, position, curve_control);

                    has_curve_control = false;
                    if (pass != 0) {
                        break;
                    }
                } else if (has_curve_start && has_curve_control
                           && !point.on_curve) {
                    double mid_x = (curve_control.x + position.x) / 2.0;
                    double mid_y = (curve_control.y + position.y) / 2.0;

                    path_builder_quad_bezier_to(
                        path,
                        geom_vec2_new(mid_x, mid_y),
                        curve_control
                    );

                    curve_control = position;
                    if (pass != 0) {
                        break;
                    }
                }
            }
        }

        x_coord = contour_end_x;
        y_coord = contour_end_y;
    }

    canvas_draw_path(canvas, path, brush);
    arena_free(temp_arena);
}
