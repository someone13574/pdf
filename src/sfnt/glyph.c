#include "glyph.h"

#include <stdint.h>

#include "arena/arena.h"
#include "canvas/canvas.h"
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
        sfnt_uint16_array_new(arena, (size_t)glyph->num_contours);
    PDF_PROPAGATE(
        sfnt_parser_read_uint16_array(parser, data->end_pts_of_contours)
    );

    PDF_PROPAGATE(sfnt_parser_read_uint16(parser, &data->instruction_len));
    data->instructions = sfnt_uint8_array_new(arena, data->instruction_len);
    PDF_PROPAGATE(sfnt_parser_read_uint8_array(parser, data->instructions));

    uint16_t last_point_idx;
    RELEASE_ASSERT(sfnt_uint16_array_get(
        data->end_pts_of_contours,
        sfnt_uint16_array_len(data->end_pts_of_contours) - 1,
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

        point_idx += flags.repetitions + 1;
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
            LOG_DIAG(TRACE, SFNT, "Delta-x: %d", 0);
            sfnt_glyph_point_array_set(
                data->points,
                x_coord_idx++,
                (SfntGlyphPoint) {.on_curve = on_curve, .delta_x = 0}
            );
            continue;
        }

        // Repeat
        for (size_t repetition = 0; repetition < flags.repetitions + 1;
             repetition++) {
            if ((flags.flags & SFNT_SIMPLE_GLYPH_X_SHORT) != 0) {
                bool positive =
                    (flags.flags & SFNT_SIMPLE_GLYPH_X_MODIFIER) != 0;
                uint8_t delta_mag;
                PDF_PROPAGATE(sfnt_parser_read_uint8(parser, &delta_mag));

                int16_t delta_x =
                    (int16_t)(positive ? delta_mag : -(int16_t)delta_mag);
                LOG_DIAG(TRACE, SFNT, "Delta-x: %d", delta_x);
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
            LOG_DIAG(TRACE, SFNT, "Delta-y: %d", 0);

            SfntGlyphPoint* point;
            RELEASE_ASSERT(sfnt_glyph_point_array_get_ptr(
                data->points,
                y_coord_idx++,
                &point
            ));
            point->delta_y = 0;

            continue;
        }

        // Repeat
        for (size_t repetition = 0; repetition < flags.repetitions + 1;
             repetition++) {
            if ((flags.flags & SFNT_SIMPLE_GLYPH_Y_SHORT) != 0) {
                bool positive =
                    (flags.flags & SFNT_SIMPLE_GLYPH_Y_MODIFIER) != 0;
                uint8_t delta_mag;
                PDF_PROPAGATE(sfnt_parser_read_uint8(parser, &delta_mag));

                int16_t delta_y =
                    (int16_t)(positive ? delta_mag : -(int16_t)delta_mag);
                LOG_DIAG(TRACE, SFNT, "Delta-y: %d", delta_y);
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

Canvas* sfnt_glyph_render(Arena* arena, SfntGlyph* glyph, uint32_t resolution) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(glyph);
    RELEASE_ASSERT(resolution > 0);

    Canvas* canvas = canvas_new(arena, resolution, resolution, 0xffffffff);

    if (glyph->glyph_type != SFNT_GLYPH_TYPE_SIMPLE) {
        LOG_TODO("Only simple glyphs are supported");
    }

    SfntSimpleGlyph data = glyph->data.simple;

    int32_t x_range = glyph->x_max - glyph->x_min;
    int32_t y_range = glyph->y_max - glyph->y_min;
    int32_t max_range = x_range > y_range ? x_range : y_range;

    double scale = (max_range > 0)
        ? ((double)(resolution - 1) / (double)max_range) / 2.0
        : 1.0;
    double offset_x = ((double)resolution - 1.0 - x_range * scale) * 0.5;
    double offset_y = ((double)resolution - 1.0 - y_range * scale) * 0.5;

    int32_t x_coord = 0;
    int32_t y_coord = 0;
    uint16_t contour_start = 0;

    for (size_t contour_idx = 0;
         contour_idx < sfnt_uint16_array_len(data.end_pts_of_contours);
         contour_idx++) {
        uint16_t contour_end;
        RELEASE_ASSERT(sfnt_uint16_array_get(
            data.end_pts_of_contours,
            contour_idx,
            &contour_end
        ));

        if (contour_idx != 0) {
            RELEASE_ASSERT(sfnt_uint16_array_get(
                data.end_pts_of_contours,
                contour_idx - 1,
                &contour_start
            ));
            contour_start++;
        }

        bool has_prev_point = false;
        bool has_first_point = false;
        double prev_x, prev_y, first_x, first_y;
        for (size_t point_idx = contour_start; point_idx <= contour_end;
             point_idx++) {
            SfntGlyphPoint point;
            RELEASE_ASSERT(
                sfnt_glyph_point_array_get(data.points, point_idx, &point)
            );

            x_coord += point.delta_x;
            y_coord += point.delta_y;

            double x = (x_coord - glyph->x_min) * scale + offset_x;
            double y = (y_coord - glyph->y_min) * scale + offset_y;
            y = (double)(resolution - 1) - y;

            canvas_draw_circle(
                canvas,
                x,
                y,
                10.0,
                point.on_curve ? 0xff0000ff : 0x00ff00ff
            );

            if (point.on_curve) {
                if (has_prev_point) {
                    canvas_draw_line(
                        canvas,
                        x,
                        y,
                        prev_x,
                        prev_y,
                        1.0,
                        2.0,
                        0x000000ff
                    );
                }

                if (!has_first_point) {
                    has_first_point = true;
                    first_x = x;
                    first_y = y;
                }

                has_prev_point = true;
                prev_x = x;
                prev_y = y;
            }
        }

        if (has_first_point) {
            canvas_draw_line(
                canvas,
                prev_x,
                prev_y,
                first_x,
                first_y,
                1.0,
                2.0,
                0x000000ff
            );
        }
    }

    return canvas;
}
