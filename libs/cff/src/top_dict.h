#pragma once

#include <stdbool.h>

#include "err/error.h"
#include "geom/mat3.h"
#include "geom/rect.h"
#include "parse_ctx/ctx.h"
#include "types.h"

typedef struct {
    CffSID version;
    CffSID notice;
    CffSID copyright;
    CffSID full_name;
    CffSID family_name;
    CffSID weight;
    bool is_fixed_pitch;
    CffNumber italic_angle;
    CffNumber underline_position;
    CffNumber underline_thickness;
    int32_t paint_type;
    int32_t charstring_type;
    GeomMat3 font_matrix;
    int32_t unique_id;
    GeomRect font_bbox;
    CffNumber stroke_width;
    // TODO: XUID
    int32_t charset;
    int32_t encoding;
    int32_t char_strings;
    int32_t private_dict_size;
    int32_t private_offset;
    int32_t synthetic_base;
    CffSID postscript;
    CffSID base_font_name;
    CffSID base_font_blend;
} CffTopDict;

Error*
cff_parse_top_dict(ParseCtx* ctx, size_t length, CffTopDict* top_dict_out);

CffTopDict cff_top_dict_default(void);
