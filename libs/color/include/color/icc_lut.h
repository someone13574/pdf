#pragma once

#include <stdint.h>

#include "color/icc.h"
#include "color/icc_color.h"
#include "err/error.h"
#include "geom/mat3.h"
#include "parse_ctx/ctx.h"

typedef struct {
    uint32_t signature;
    uint32_t reserved;
    uint8_t input_channels;
    uint8_t output_channels;
    uint8_t grid_points;
    uint8_t padding;
    ICCS15Fixed16Number e1;
    ICCS15Fixed16Number e2;
    ICCS15Fixed16Number e3;
    ICCS15Fixed16Number e4;
    ICCS15Fixed16Number e5;
    ICCS15Fixed16Number e6;
    ICCS15Fixed16Number e7;
    ICCS15Fixed16Number e8;
    ICCS15Fixed16Number e9;

    GeomMat3 matrix;
} ICCLutCommon;

typedef struct {
    ParseCtx ctx;
    ICCLutCommon common;
    ParseCtx input_tables;
    ParseCtx clut_values;
    ParseCtx output_tables;

    GeomMat3 matrix;
} ICCLut8;

Error* icc_parse_lut8(ParseCtx ctx, ICCLut8* out);
Error* icc_lut8_map(
    ICCLut8* lut,
    ICCColor input,
    double out[15],
    size_t* output_channels
);
