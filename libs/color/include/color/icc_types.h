#pragma once

#include <stdint.h>

#include "geom/vec3.h"
#include "parse_ctx/ctx.h"

typedef struct {
    uint16_t year;
    uint16_t month;
    uint16_t day_of_month;
    uint16_t hour;
    uint16_t minute;
    uint16_t second;
} IccDateTime;

Error* icc_parse_date_time(ParseCtx* ctx, IccDateTime* out);

typedef uint16_t IccU8Fixed8Number;
double icc_u8_fixed8_to_double(IccU8Fixed8Number num);

typedef int32_t IccS15Fixed16Number;
double icc_s15_fixed16_to_double(IccS15Fixed16Number num);

typedef struct {
    IccS15Fixed16Number x;
    IccS15Fixed16Number y;
    IccS15Fixed16Number z;
} IccXYZNumber;

Error* icc_parse_xyz_number(ParseCtx* ctx, IccXYZNumber* out);
GeomVec3 icc_xyz_number_to_geom(IccXYZNumber xyz);
