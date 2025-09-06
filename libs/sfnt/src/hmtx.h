#pragma once

#include <stdint.h>

#include "arena/arena.h"
#include "hhea.h"
#include "maxp.h"
#include "parser.h"
#include "pdf_error/error.h"
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

PdfError* sfnt_parse_hmtx(
    Arena* arena,
    SfntParser* parser,
    const SfntMaxp* maxp,
    const SfntHhea* hhea,
    SfntHmtx* hmtx
);
