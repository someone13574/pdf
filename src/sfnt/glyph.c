#include "glyph.h"

#include <stdint.h>

#include "arena/arena.h"
#include "canvas/canvas.h"
#include "canvas/path_builder.h"
#include "geom/vec3.h"
#include "logger/log.h"
#include "parser.h"
#include "pdf_error/error.h"
#include "sfnt/glyph.h"

#define DVEC_NAME SfntSimpleGlyphFlagsVec
#define DVEC_LOWERCASE_NAME sfnt_simple_glyph_flags_vec
#define DVEC_TYPE SfntSimpleGlyphFlags
#include "arena/dvec_impl.h"

#define DARRAY_NAME SfntGlyphPointArray
#define DARRAY_LOWERCASE_NAME sfnt_glyph_point_array
#define DARRAY_TYPE SfntGlyphPoint
#include "arena/darray_impl.h"

PdfError*
sfnt_parse_simple_glyph(Arena* arena, SfntParser* parser, SfntGlyph* glyph) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(glyph);
    RELEASE_ASSERT(glyph->num_contours > 0);

    SfntSimpleGlyph* data = &glyph->data.simple;

    data->end_pts_of_contours =
        uint16_array_new(arena, (size_t)glyph->num_contours);
    PDF_PROPAGATE(
        sfnt_parser_read_uint16_array(parser, data->end_pts_of_contours)
    );

    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &data->instruction_len));
    data->instructions = uint8_array_new(arena, data->instruction_len);
    PDF_PROPAGATE(sfnt_parser_read_uint8_array(parser, data->instructions));

    uint16_t last_point_idx;
    RELEASE_ASSERT(uint16_array_get(
        data->end_pts_of_contours,
        uint16_array_len(data->end_pts_of_contours) - 1,
        &last_point_idx
    ));

    // Parse flags
    uint16_t point_idx = 0;
    data->flags = sfnt_simple_glyph_flags_vec_new(arena);

    while (point_idx <= last_point_idx) {
        SfntSimpleGlyphFlags flags = {.flags = 0, .repetitions = 0};
        PDF_PROPAGATE(sfnt_parser_read_uint8(parser, &flags.flags));

        if ((flags.flags & SFNT_SIMPLE_GLYPH_REPEAT) != 0) {
            PDF_PROPAGATE(sfnt_parser_read_uint8(parser, &flags.repetitions));
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
                PDF_PROPAGATE(sfnt_parser_read_uint8(parser, &delta_mag));

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
                PDF_PROPAGATE(sfnt_parser_read_int16(parser, &delta_x));

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
                PDF_PROPAGATE(sfnt_parser_read_uint8(parser, &delta_mag));

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
                PDF_PROPAGATE(sfnt_parser_read_int16(parser, &delta_y));

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

PdfError* sfnt_parse_glyph(Arena* arena, SfntParser* parser, SfntGlyph* glyph) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(glyph);

    PDF_PROPAGATE(sfnt_parser_read_int16(parser, &glyph->num_contours));
    PDF_PROPAGATE(sfnt_parser_read_fword(parser, &glyph->x_min));
    PDF_PROPAGATE(sfnt_parser_read_fword(parser, &glyph->y_min));
    PDF_PROPAGATE(sfnt_parser_read_fword(parser, &glyph->x_max));
    PDF_PROPAGATE(sfnt_parser_read_fword(parser, &glyph->y_max));

    if (glyph->num_contours == 0) {
        glyph->glyph_type = SFNT_GLYPH_TYPE_NONE;
        return NULL;
    }

    if (glyph->num_contours > 0) {
        glyph->glyph_type = SFNT_GLYPH_TYPE_SIMPLE;
        return sfnt_parse_simple_glyph(arena, parser, glyph);
    }

    LOG_TODO("Compound glyphs");
}

void sfnt_glyph_render(Canvas* canvas, SfntGlyph* glyph, GeomMat3 transform) {
    RELEASE_ASSERT(canvas);
    RELEASE_ASSERT(glyph);

    Arena* temp_arena = arena_new(4096);
    PathBuilder* path = path_builder_new(temp_arena);

    if (glyph->glyph_type != SFNT_GLYPH_TYPE_SIMPLE) {
        LOG_TODO("Only simple glyphs are supported");
    }

    SfntSimpleGlyph data = glyph->data.simple;

    int32_t x_coord = 0;
    int32_t y_coord = 0;

    for (size_t contour_idx = 0;
         contour_idx < uint16_array_len(data.end_pts_of_contours);
         contour_idx++) {
        uint16_t contour_end;
        RELEASE_ASSERT(uint16_array_get(
            data.end_pts_of_contours,
            contour_idx,
            &contour_end
        ));

        uint16_t contour_start = 0;
        if (contour_idx != 0) {
            RELEASE_ASSERT(uint16_array_get(
                data.end_pts_of_contours,
                contour_idx - 1,
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
        double curve_control_x = 0.0, curve_control_y = 0.0;

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

                GeomVec3 position = geom_vec3_transform(
                    geom_vec3_new((double)x_coord, (double)y_coord, 1.0),
                    transform
                );

                if (!has_curve_start && point.on_curve) {
                    has_curve_start = true;
                    path_builder_new_contour(path, position.x, position.y);
                } else if (has_curve_start && !has_curve_control && !point.on_curve) {
                    has_curve_control = true;
                    curve_control_x = position.x;
                    curve_control_y = position.y;
                } else if (has_curve_start && !has_curve_control && point.on_curve) {
                    path_builder_line_to(path, position.x, position.y);

                    if (pass != 0) {
                        break;
                    }
                } else if (has_curve_start && has_curve_control && point.on_curve) {
                    path_builder_bezier_to(
                        path,
                        position.x,
                        position.y,
                        curve_control_x,
                        curve_control_y
                    );

                    has_curve_control = false;
                    if (pass != 0) {
                        break;
                    }
                } else if (has_curve_start && has_curve_control && !point.on_curve) {
                    double mid_x = (curve_control_x + position.x) / 2.0;
                    double mid_y = (curve_control_y + position.y) / 2.0;

                    path_builder_bezier_to(
                        path,
                        mid_x,
                        mid_y,
                        curve_control_x,
                        curve_control_y
                    );

                    curve_control_x = position.x;
                    curve_control_y = position.y;
                    if (pass != 0) {
                        break;
                    }
                }
            }
        }

        x_coord = contour_end_x;
        y_coord = contour_end_y;
    }

    canvas_draw_path(canvas, path);
    arena_free(temp_arena);
}
