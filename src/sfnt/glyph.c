#include "glyph.h"

#include <stdint.h>

#include "arena/arena.h"
#include "log.h"
#include "parser.h"
#include "pdf/result.h"

#define DVEC_NAME SfntSimpleGlyphFlagsVec
#define DVEC_LOWERCASE_NAME sfnt_simple_glyph_flags_vec
#define DVEC_TYPE SfntSimpleGlyphFlags
#include "../arena/dvec_impl.h"

PdfResult
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
    size_t num_x_coords = 0;
    size_t num_y_coords = 0;
    data->flags = sfnt_simple_glyph_flags_vec_new(arena);

    while (point_idx <= last_point_idx) {
        SfntSimpleGlyphFlags flags = {.flags = 0, .repetitions = 0};
        PDF_PROPAGATE(sfnt_parser_read_uint8(parser, &flags.flags));

        if ((flags.flags & SFNT_SIMPLE_GLYPH_REPEAT) != 0) {
            PDF_PROPAGATE(sfnt_parser_read_uint8(parser, &flags.repetitions));
        }

        if ((flags.flags & SFNT_SIMPLE_GLYPH_X_SHORT) != 0
            || (flags.flags & SFNT_SIMPLE_GLYPH_X_MODIFIER) == 0) {
            num_x_coords += flags.repetitions + 1;
        }

        if ((flags.flags & SFNT_SIMPLE_GLYPH_Y_SHORT) != 0
            || (flags.flags & SFNT_SIMPLE_GLYPH_Y_MODIFIER) == 0) {
            num_y_coords += flags.repetitions + 1;
        }

        point_idx += flags.repetitions + 1;
        sfnt_simple_glyph_flags_vec_push(data->flags, flags);
    }

    data->x_coords = sfnt_int16_array_new(arena, num_x_coords);
    data->y_coords = sfnt_int16_array_new(arena, num_y_coords);

    // Parse x-coords
    size_t x_coord_idx = 0;
    for (size_t flag_idx = 0;
         flag_idx < sfnt_simple_glyph_flags_vec_len(data->flags);
         flag_idx++) {
        SfntSimpleGlyphFlags flags;
        sfnt_simple_glyph_flags_vec_get(data->flags, flag_idx, &flags);

        // Coordinate stays the same
        if ((flags.flags & SFNT_SIMPLE_GLYPH_X_SHORT) == 0
            && (flags.flags & SFNT_SIMPLE_GLYPH_X_MODIFIER) != 0) {
            LOG_TRACE_G("sfnt", "Delta-x: %d", 0);
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
                sfnt_int16_array_set(
                    data->x_coords,
                    x_coord_idx++,
                    (int16_t)(positive ? delta_mag : -(int16_t)delta_mag)
                );
                LOG_TRACE_G(
                    "sfnt",
                    "Delta-x: %d",
                    (int16_t)(positive ? delta_mag : -(int16_t)delta_mag)
                );
            } else {
                int16_t delta;
                PDF_PROPAGATE(sfnt_parser_read_int16(parser, &delta));
                sfnt_int16_array_set(data->x_coords, x_coord_idx++, delta);
                LOG_TRACE_G("sfnt", "Delta-x: %d", delta);
            }
        }
    }

    // Parse y-coords
    size_t y_coord_idx = 0;
    for (size_t flag_idx = 0;
         flag_idx < sfnt_simple_glyph_flags_vec_len(data->flags);
         flag_idx++) {
        SfntSimpleGlyphFlags flags;
        sfnt_simple_glyph_flags_vec_get(data->flags, flag_idx, &flags);

        // Coordinate stays the same
        if ((flags.flags & SFNT_SIMPLE_GLYPH_Y_SHORT) == 0
            && (flags.flags & SFNT_SIMPLE_GLYPH_Y_MODIFIER) != 0) {
            LOG_TRACE_G("sfnt", "Delta-y: %d", 0);
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
                sfnt_int16_array_set(
                    data->y_coords,
                    y_coord_idx++,
                    (int16_t)(positive ? delta_mag : -(int16_t)delta_mag)
                );
                LOG_TRACE_G(
                    "sfnt",
                    "Delta-y: %d",
                    (int16_t)(positive ? delta_mag : -(int16_t)delta_mag)
                );
            } else {
                int16_t delta;
                PDF_PROPAGATE(sfnt_parser_read_int16(parser, &delta));
                sfnt_int16_array_set(data->y_coords, y_coord_idx++, delta);
                LOG_TRACE_G("sfnt", "Delta-y: %d", delta);
            }
        }
    }

    return PDF_OK;
}

PdfResult sfnt_parse_glyph(Arena* arena, SfntParser* parser, SfntGlyph* glyph) {
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
        return PDF_OK;
    }

    if (glyph->num_contours > 0) {
        glyph->glyph_type = SFNT_GLYPH_TYPE_SIMPLE;
        return sfnt_parse_simple_glyph(arena, parser, glyph);
    }

    LOG_TODO("Compound glyphs");
}
