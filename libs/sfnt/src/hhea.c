#include "sfnt/hhea.h"

#include "err/error.h"
#include "logger/log.h"
#include "parse_ctx/binary.h"
#include "parse_ctx/ctx.h"
#include "sfnt/types.h"

Error* sfnt_parse_hhea(ParseCtx ctx, SfntHhea* hhea) {
    RELEASE_ASSERT(hhea);

    TRY(sfnt_read_fixed(&ctx, &hhea->version));
    if (hhea->version != 0x10000) {
        return ERROR(
            SFNT_ERR_INVALID_VERSION,
            "Invalid hhea version %u",
            hhea->version
        );
    }

    TRY(sfnt_read_fword(&ctx, &hhea->ascent));
    TRY(sfnt_read_fword(&ctx, &hhea->descent));
    TRY(sfnt_read_fword(&ctx, &hhea->line_gap));
    TRY(sfnt_read_ufword(&ctx, &hhea->advance_width_max));
    TRY(sfnt_read_fword(&ctx, &hhea->min_left_side_bearing));
    TRY(sfnt_read_fword(&ctx, &hhea->min_right_side_bearing));
    TRY(sfnt_read_fword(&ctx, &hhea->x_max_extent));
    TRY(parse_ctx_read_i16_be(&ctx, &hhea->caret_slope_rise));
    TRY(parse_ctx_read_i16_be(&ctx, &hhea->caret_slope_run));
    TRY(sfnt_read_fword(&ctx, &hhea->caret_offset));

    for (size_t idx = 0; idx < 4; idx++) {
        int16_t reserved;
        TRY(parse_ctx_read_i16_be(&ctx, &reserved));

        if (reserved != 0) {
            return ERROR(SFNT_ERR_RESERVED, "Reserved value in hhea was not 0");
        }
    }

    TRY(parse_ctx_read_i16_be(&ctx, &hhea->metric_data_format));
    if (hhea->metric_data_format != 0) {
        return ERROR(SFNT_ERR_INVALID_VERSION, "hhea metricDataFormat not 0");
    }

    TRY(parse_ctx_read_u16_be(&ctx, &hhea->num_of_long_for_metrics));

    return NULL;
}
