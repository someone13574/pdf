#include "hhea.h"

#include "err/error.h"
#include "logger/log.h"
#include "parser.h"

Error* sfnt_parse_hhea(SfntParser* parser, SfntHhea* hhea) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(hhea);

    TRY(sfnt_parser_read_fixed(parser, &hhea->version));
    if (hhea->version != 0x10000) {
        return ERROR(
            SFNT_ERR_INVALID_VERSION,
            "Invalid hhea version %u",
            hhea->version
        );
    }

    TRY(sfnt_parser_read_fword(parser, &hhea->ascent));
    TRY(sfnt_parser_read_fword(parser, &hhea->descent));
    TRY(sfnt_parser_read_fword(parser, &hhea->line_gap));
    TRY(sfnt_parser_read_ufword(parser, &hhea->advance_width_max));
    TRY(sfnt_parser_read_fword(parser, &hhea->min_left_side_bearing));
    TRY(sfnt_parser_read_fword(parser, &hhea->min_right_side_bearing));
    TRY(sfnt_parser_read_fword(parser, &hhea->x_max_extent));
    TRY(sfnt_parser_read_int16(parser, &hhea->caret_slope_rise));
    TRY(sfnt_parser_read_int16(parser, &hhea->caret_slope_run));
    TRY(sfnt_parser_read_fword(parser, &hhea->caret_offset));

    for (size_t idx = 0; idx < 4; idx++) {
        int16_t reserved;
        TRY(sfnt_parser_read_int16(parser, &reserved));

        if (reserved != 0) {
            return ERROR(SFNT_ERR_RESERVED, "Reserved value in hhea was not 0");
        }
    }

    TRY(sfnt_parser_read_int16(parser, &hhea->metric_data_format));
    if (hhea->metric_data_format != 0) {
        return ERROR(SFNT_ERR_INVALID_VERSION, "hhea metricDataFormat not 0");
    }

    TRY(sfnt_parser_read_uint16(parser, &hhea->num_of_long_for_metrics));

    return NULL;
}
