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

    ParseCtx input_table;
    ParseCtx clut;
    ParseCtx output_table;
} ICCLut8;

Error* icc_parse_lut8(ParseCtx ctx, ICCLut8* out);
Error* icc_lut8_map(ICCLut8 lut, ICCColor input, double out[15]);

typedef struct {
    ParseCtx ctx;
    ICCLutCommon common;

    uint16_t input_entries;
    uint16_t output_entries;

    ParseCtx input_table;
    ParseCtx clut;
    ParseCtx output_table;
} ICCLut16;

Error* icc_parse_lut16(ParseCtx ctx, ICCLut16* out);
Error* icc_lut16_map(ICCLut16 lut, ICCColor input, double out[15]);
