#include "color/icc_types.h"

#include "logger/log.h"

Error* icc_parse_date_time(ParseCtx* ctx, IccDateTime* out) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(out);

    TRY(parse_ctx_read_u16_be(ctx, &out->year));
    TRY(parse_ctx_read_u16_be(ctx, &out->month));
    TRY(parse_ctx_read_u16_be(ctx, &out->day_of_month));
    TRY(parse_ctx_read_u16_be(ctx, &out->hour));
    TRY(parse_ctx_read_u16_be(ctx, &out->minute));
    TRY(parse_ctx_read_u16_be(ctx, &out->second));

    return NULL;
}

double icc_u8_fixed8_to_double(IccU8Fixed8Number num) {
    return (double)num / 256.0;
}

double icc_s15_fixed16_to_double(IccS15Fixed16Number num) {
    return (double)num / 65536.0;
}

Error* icc_parse_xyz_number(ParseCtx* ctx, IccXYZNumber* out) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(out);

    TRY(parse_ctx_read_i32_be(ctx, &out->x));
    TRY(parse_ctx_read_i32_be(ctx, &out->y));
    TRY(parse_ctx_read_i32_be(ctx, &out->z));

    return NULL;
}

GeomVec3 icc_xyz_number_to_geom(IccXYZNumber xyz) {
    return geom_vec3_new(
        icc_s15_fixed16_to_double(xyz.x),
        icc_s15_fixed16_to_double(xyz.y),
        icc_s15_fixed16_to_double(xyz.z)
    );
}
