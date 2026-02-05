#pragma once

#include <stdint.h>

#include "arena/arena.h"
#include "err/error.h"
#include "parse_ctx/ctx.h"
#include "sfnt/hhea.h"
#include "sfnt/maxp.h"
#include "sfnt/types.h"

typedef struct {
    uint16_t advance_width;
    int16_t left_side_bearing;
} SfntLongHorMetric;

#define DARRAY_NAME SfntHMetricsArray
#define DARRAY_LOWERCASE_NAME sfnt_hmetrics_array
#define DARRAY_TYPE SfntLongHorMetric
#include "arena/darray_decl.h"

/// Horizontal metrics table
typedef struct {
    SfntHMetricsArray* h_metrics;
    SfntFWordArray* left_side_bearing;
} SfntHmtx;

Error* sfnt_parse_hmtx(
    Arena* arena,
    ParseCtx ctx,
    const SfntMaxp* maxp,
    const SfntHhea* hhea,
    SfntHmtx* hmtx
);
