#pragma once

#include "parse_ctx/ctx.h"
#include "sfnt/types.h"

typedef struct {
    SfntFixed version;
    SfntFixed font_revision;
    uint32_t check_sum_adjustment;
    uint16_t flags;
    uint16_t units_per_em;
    SfntLongDateTime created;
    SfntLongDateTime modified;
    SfntFWord x_min;
    SfntFWord y_min;
    SfntFWord x_max;
    SfntFWord y_max;
    uint16_t mac_style;
    uint16_t lowest_rec_ppem;
    int16_t front_direction_hint;
    int16_t idx_to_loc_format;
    int16_t glyph_data_format;
} SfntHead;

Error* sfnt_parse_head(ParseCtx parser, SfntHead* head);
