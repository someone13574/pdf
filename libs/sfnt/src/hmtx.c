#include "sfnt/hmtx.h"

#include "err/error.h"
#include "logger/log.h"
#include "parse_ctx/binary.h"
#include "parse_ctx/ctx.h"
#include "sfnt/types.h"

#define DARRAY_NAME SfntHMetricsArray
#define DARRAY_LOWERCASE_NAME sfnt_hmetrics_array
#define DARRAY_TYPE SfntLongHorMetric
#include "arena/darray_impl.h"

Error* sfnt_parse_hmtx(
    Arena* arena,
    ParseCtx ctx,
    const SfntMaxp* maxp,
    const SfntHhea* hhea,
    SfntHmtx* hmtx
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(maxp);
    RELEASE_ASSERT(hhea);
    RELEASE_ASSERT(hmtx);

    hmtx->h_metrics =
        sfnt_hmetrics_array_new(arena, hhea->num_of_long_for_metrics);
    for (size_t idx = 0; idx < hhea->num_of_long_for_metrics; idx++) {
        SfntLongHorMetric long_hor_metric;
        TRY(parse_ctx_read_u16_be(&ctx, &long_hor_metric.advance_width));
        TRY(parse_ctx_read_i16_be(&ctx, &long_hor_metric.left_side_bearing));

        sfnt_hmetrics_array_set(hmtx->h_metrics, idx, long_hor_metric);
    }

    hmtx->left_side_bearing = sfnt_fword_array_new(
        arena,
        maxp->num_glyphs - hhea->num_of_long_for_metrics
    );
    for (size_t idx = 0; idx < sfnt_fword_array_len(hmtx->left_side_bearing);
         idx++) {
        SfntFWord* left_side_bearing = NULL;
        RELEASE_ASSERT(sfnt_fword_array_get_ptr(
            hmtx->left_side_bearing,
            idx,
            &left_side_bearing
        ));
        TRY(sfnt_read_fword(&ctx, left_side_bearing));
    }

    return NULL;
}
