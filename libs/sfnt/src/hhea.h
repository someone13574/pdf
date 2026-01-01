#pragma once

#include <stdint.h>

#include "err/error.h"
#include "parser.h"
#include "sfnt/types.h"

/// Horizontal layout information
typedef struct {
    SfntFixed version;
    SfntFWord ascent;
    SfntFWord descent;
    SfntFWord line_gap;
    SfntUFWord advance_width_max;
    SfntFWord min_left_side_bearing;
    SfntFWord min_right_side_bearing;
    SfntFWord x_max_extent;
    int16_t caret_slope_rise;
    int16_t caret_slope_run;
    SfntFWord caret_offset;
    int16_t metric_data_format;
    uint16_t num_of_long_for_metrics;
} SfntHhea;

Error* sfnt_parse_hhea(SfntParser* parser, SfntHhea* hhea);
