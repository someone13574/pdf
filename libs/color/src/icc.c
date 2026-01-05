#include "color/icc.h"

#include <stdint.h>

#include "err/error.h"
#include "geom/vec3.h"
#include "logger/log.h"
#include "parse_ctx/ctx.h"

Error* icc_parse_date_time(ParseCtx* ctx, ICCDateTime* out) {
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

double icc_s15_fixed16_to_double(ICCS15Fixed16Number num) {
    return (double)num / 65535.0;
}

Error* icc_parse_xyz_number(ParseCtx* ctx, ICCXYZNumber* out) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(out);

    TRY(parse_ctx_read_u32_be(ctx, &out->x));
    TRY(parse_ctx_read_u32_be(ctx, &out->y));
    TRY(parse_ctx_read_u32_be(ctx, &out->z));

    return NULL;
}

GeomVec3 icc_xyz_number_to_geom(ICCXYZNumber xyz) {
    return geom_vec3_new(
        icc_s15_fixed16_to_double(xyz.x),
        icc_s15_fixed16_to_double(xyz.y),
        icc_s15_fixed16_to_double(xyz.z)
    );
}

Error* icc_parse_header(ParseCtx* ctx, ICCProfileHeader* out) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(out);
    RELEASE_ASSERT(ctx->offset == 0);

    TRY(parse_ctx_bound_check(ctx, 128));
    TRY(parse_ctx_read_u32_be(ctx, &out->size));
    TRY(parse_ctx_read_u32_be(ctx, &out->preferred_cmm));
    TRY(parse_ctx_read_u32_be(ctx, &out->version));
    TRY(parse_ctx_read_u32_be(ctx, &out->class));
    TRY(parse_ctx_read_u32_be(ctx, &out->color_space));
    TRY(parse_ctx_read_u32_be(ctx, &out->pcs));
    TRY(icc_parse_date_time(ctx, &out->date));
    TRY(parse_ctx_read_u32_be(ctx, &out->signature));
    TRY(parse_ctx_read_u32_be(ctx, &out->platform_signature));
    TRY(parse_ctx_read_u32_be(ctx, &out->flags));
    TRY(parse_ctx_read_u32_be(ctx, &out->device_manufacturer));
    TRY(parse_ctx_read_u32_be(ctx, &out->device_model));
    TRY(parse_ctx_read_u64_be(ctx, &out->device_attributes));
    TRY(parse_ctx_read_u32_be(ctx, &out->rendering_intent));
    TRY(icc_parse_xyz_number(ctx, &out->illuminant));
    TRY(parse_ctx_read_u32_be(ctx, &out->creator_signature));
    TRY(parse_ctx_read_u64_be(ctx, &out->id_low));
    TRY(parse_ctx_read_u64_be(ctx, &out->id_high));

    RELEASE_ASSERT(ctx->offset == 100);
    TRY(parse_ctx_seek(ctx, 128));

    return NULL;
}

Error* icc_tag_table_new(ParseCtx* file_ctx, ICCTagTable* out) {
    RELEASE_ASSERT(file_ctx);
    RELEASE_ASSERT(out);

    out->file_ctx = *file_ctx;
    TRY(parse_ctx_seek(file_ctx, 128));
    TRY(parse_ctx_read_u32_be(file_ctx, &out->tag_count));
    TRY(parse_ctx_new_subctx(
        file_ctx,
        128,
        4 + 12 * out->tag_count,
        &out->table_ctx
    ));

    return NULL;
}

Error*
icc_tag_table_lookup(ICCTagTable table, uint32_t tag_signature, ParseCtx* out) {
    RELEASE_ASSERT(out);

    for (uint32_t idx = 0; idx < table.tag_count; idx++) {
        uint32_t entry_signature;
        TRY(parse_ctx_seek(&table.table_ctx, 4 + 12 * idx));
        TRY(parse_ctx_read_u32_be(&table.table_ctx, &entry_signature));

        if (tag_signature == entry_signature) {
            uint32_t offset, size;
            TRY(parse_ctx_read_u32_be(&table.table_ctx, &offset));
            TRY(parse_ctx_read_u32_be(&table.table_ctx, &size));
            TRY(parse_ctx_new_subctx(
                &table.file_ctx,
                (size_t)offset,
                (size_t)size,
                out
            ));
            break;
        }
    }

    return NULL;
}
