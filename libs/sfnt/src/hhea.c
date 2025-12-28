#include "hhea.h"

#include "logger/log.h"
#include "parser.h"
#include "pdf_error/error.h"

PdfError* sfnt_parse_hhea(SfntParser* parser, SfntHhea* hhea) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(hhea);

    PDF_PROPAGATE(sfnt_parser_read_fixed(parser, &hhea->version));
    if (hhea->version != 0x10000) {
        return PDF_ERROR(
            PDF_ERR_SFNT_INVALID_VERSION,
            "Invalid hhea version %u",
            hhea->version
        );
    }

    PDF_PROPAGATE(sfnt_parser_read_fword(parser, &hhea->ascent));
    PDF_PROPAGATE(sfnt_parser_read_fword(parser, &hhea->descent));
    PDF_PROPAGATE(sfnt_parser_read_fword(parser, &hhea->line_gap));
    PDF_PROPAGATE(sfnt_parser_read_ufword(parser, &hhea->advance_width_max));
    PDF_PROPAGATE(sfnt_parser_read_fword(parser, &hhea->min_left_side_bearing));
    PDF_PROPAGATE(
        sfnt_parser_read_fword(parser, &hhea->min_right_side_bearing)
    );
    PDF_PROPAGATE(sfnt_parser_read_fword(parser, &hhea->x_max_extent));
    PDF_PROPAGATE(sfnt_parser_read_int16(parser, &hhea->caret_slope_rise));
    PDF_PROPAGATE(sfnt_parser_read_int16(parser, &hhea->caret_slope_run));
    PDF_PROPAGATE(sfnt_parser_read_fword(parser, &hhea->caret_offset));

    for (size_t idx = 0; idx < 4; idx++) {
        int16_t reserved;
        PDF_PROPAGATE(sfnt_parser_read_int16(parser, &reserved));

        if (reserved != 0) {
            return PDF_ERROR(
                PDF_ERR_SFNT_RESERVED,
                "Reserved value in hhea was not 0"
            );
        }
    }

    PDF_PROPAGATE(sfnt_parser_read_int16(parser, &hhea->metric_data_format));
    if (hhea->metric_data_format != 0) {
        return PDF_ERROR(
            PDF_ERR_SFNT_INVALID_VERSION,
            "hhea metricDataFormat not 0"
        );
    }

    PDF_PROPAGATE(
        sfnt_parser_read_uint16(parser, &hhea->num_of_long_for_metrics)
    );

    return NULL;
}
