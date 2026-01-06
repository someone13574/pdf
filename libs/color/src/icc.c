#include "color/icc.h"

#include <stdint.h>

#include "color/cie.h"
#include "color/icc_color.h"
#include "color/icc_tags.h"
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

    if (out->class == 0x6C696E6B) {
        LOG_PANIC("DeviceLink profiles aren't supported");
    }

    ICCColorSpace pcs = icc_color_space_from_signature(out->pcs);
    if (pcs != ICC_COLOR_SPACE_XYZ && pcs != ICC_COLOR_SPACE_LAB) {
        return ERROR(
            ICC_ERR_INVALID_HEADER,
            "Profile connection space must be xyz or lab"
        );
    }

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

Error* icc_parse_profile(ParseCtx ctx, ICCProfile* profile) {
    RELEASE_ASSERT(profile);

    TRY(icc_parse_header(&ctx, &profile->header));
    TRY(icc_tag_table_new(&ctx, &profile->tag_table));

    return NULL;
}

static Error* icc_media_whitepoint(ICCProfile profile, ICCXYZNumber* out) {
    RELEASE_ASSERT(out);

    ParseCtx ctx;
    TRY(icc_tag_table_lookup(
        profile.tag_table,
        icc_tag_signature(ICC_TAG_MEDIA_WHITEPOINT),
        &ctx
    ));
    TRY(icc_parse_xyz_number(&ctx, out));

    return NULL;
}

Error* icc_connect_pcs(
    ICCProfile src_profile,
    ICCProfile dst_profile,
    bool src_is_absolute,
    ICCRenderingIntent intent,
    GeomVec3 src,
    GeomVec3* dst
) {
    RELEASE_ASSERT(dst);

    bool src_is_xyz = src_profile.header.pcs
                   == icc_color_space_signature(ICC_COLOR_SPACE_XYZ);
    bool intermediate_is_xyz = src_is_xyz
                            || intent == ICC_RENDERING_INTENT_ICC_ABSOLUTE
                            || src_is_absolute;
    bool dst_is_xyz = dst_profile.header.pcs
                   == icc_color_space_signature(ICC_COLOR_SPACE_XYZ);

    GeomVec3 intermediate;
    const CieXYZ D50 = {.x = 0.9642, .y = 1.0, .z = 0.8249};

    if (intermediate_is_xyz && !src_is_xyz) {
        intermediate =
            cie_xyz_to_geom(cie_lab_to_cie_xyz(cie_lab_from_geom(src), D50));
    } else {
        intermediate = src;
    }

    if (src_is_absolute && intent == ICC_RENDERING_INTENT_ICC_ABSOLUTE) {
        // Absolute -> absolute
        ICCXYZNumber src_mw, dst_mw;
        TRY(icc_media_whitepoint(src_profile, &src_mw));
        TRY(icc_media_whitepoint(dst_profile, &dst_mw));
        intermediate = geom_vec3_mul(
            intermediate,
            geom_vec3_div(
                icc_xyz_number_to_geom(dst_mw),
                icc_xyz_number_to_geom(src_mw)
            )
        );
    } else if (intent == ICC_RENDERING_INTENT_ICC_ABSOLUTE) {
        // Relative -> absolute
        ICCXYZNumber dst_mw;
        TRY(icc_media_whitepoint(dst_profile, &dst_mw));
        intermediate = geom_vec3_mul(
            intermediate,
            geom_vec3_div(icc_xyz_number_to_geom(dst_mw), cie_xyz_to_geom(D50))
        );
    } else if (src_is_absolute) {
        // Absolute -> relative
        ICCXYZNumber src_mw;
        TRY(icc_media_whitepoint(src_profile, &src_mw));
        intermediate = geom_vec3_mul(
            intermediate,
            geom_vec3_div(cie_xyz_to_geom(D50), icc_xyz_number_to_geom(src_mw))
        );
    }

    if (intermediate_is_xyz && !dst_is_xyz) {
        *dst = cie_lab_to_geom(
            cie_xyz_to_cie_lab(cie_xyz_from_geom(intermediate), D50)
        );
    } else if (!intermediate_is_xyz && dst_is_xyz) {
        *dst = cie_xyz_to_geom(
            cie_lab_to_cie_xyz(cie_lab_from_geom(intermediate), D50)
        );
    } else {
        *dst = intermediate;
    }

    return NULL;
}
