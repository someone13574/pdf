#pragma once

#include <stdint.h>

#include "arena/arena.h"
#include "color/icc_color.h"
#include "color/icc_curve.h"
#include "color/icc_types.h"
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
    IccS15Fixed16Number e1;
    IccS15Fixed16Number e2;
    IccS15Fixed16Number e3;
    IccS15Fixed16Number e4;
    IccS15Fixed16Number e5;
    IccS15Fixed16Number e6;
    IccS15Fixed16Number e7;
    IccS15Fixed16Number e8;
    IccS15Fixed16Number e9;

    GeomMat3 matrix;
} IccStandardLutCommon;

typedef struct {
    ParseCtx ctx;
    IccStandardLutCommon common;

    ParseCtx input_table;
    ParseCtx clut;
    ParseCtx output_table;
} IccLut8;

Error* icc_parse_lut8(ParseCtx ctx, IccLut8* out);
Error* icc_lut8_map(IccLut8 lut, IccColor input, double out[15]);

typedef struct {
    ParseCtx ctx;
    IccStandardLutCommon common;

    uint16_t input_entries;
    uint16_t output_entries;

    ParseCtx input_table;
    ParseCtx clut;
    ParseCtx output_table;
} IccLut16;

Error* icc_parse_lut16(ParseCtx ctx, IccLut16* out);
Error* icc_lut16_map(IccLut16 lut, IccColor input, double out[15]);

typedef struct {
    uint8_t grid_points[16];
    uint8_t precision;
    uint16_t padding;
    uint8_t padding2;
    ParseCtx data;
} IccVariableCLut;

typedef struct {
    uint32_t signature;
    uint32_t reserved;
    uint8_t input_channels;
    uint8_t output_channels;
    uint16_t padding;
    uint32_t b_curves_offset;
    uint32_t matrix_offset;
    uint32_t m_curves_offset;
    uint32_t clut_offset;
    uint32_t a_curves_offset;

    bool has_matrix;
    bool has_clut;

    IccAnyCurve b_curves[3];
    IccAnyCurve m_curves[3];
    IccAnyCurveVec* a_curves;

    GeomMat3 matrix;
    GeomVec3 matrix_vec;

    IccVariableCLut clut;
} IccLutBToA;

Error* icc_parse_lut_b_to_a(Arena* arena, ParseCtx ctx, IccLutBToA* out);
Error*
icc_lut_b_to_a_map(const IccLutBToA* lut, IccPcsColor input, double out[15]);
