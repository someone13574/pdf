#include "color/icc.h"

#include <stdint.h>

#include "color/cie.h"
#include "color/icc_color.h"
#include "color/icc_lut.h"
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

bool icc_profile_is_pcsxyz(ICCProfile profile) {
    return profile.header.pcs == icc_color_space_signature(ICC_COLOR_SPACE_XYZ);
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

Error* icc_device_to_pcs(
    ICCProfile profile,
    ICCRenderingIntent rendering_intent,
    ICCColor color,
    ICCPcsColor* output
) {
    RELEASE_ASSERT(output);

    if (color.color_space
        != icc_color_space_from_signature(profile.header.color_space)) {
        return ERROR(
            ICC_ERR_INCORRECT_SPACE,
            "Input color space %d doesn't match profile's color space %d",
            (int)color.color_space,
            (int)icc_color_space_from_signature(profile.header.color_space)
        );
    }

    ICCTag a_to_b_tag;
    switch (rendering_intent) {
        case ICC_INTENT_MEDIA_RELATIVE:
        case ICC_INTENT_ABSOLUTE: {
            a_to_b_tag = ICC_TAG_A_TO_B1;
            break;
        }
        case ICC_INTENT_PERCEPTUAL: {
            a_to_b_tag = ICC_TAG_A_TO_B0;
            break;
        }
        case ICC_INTENT_SATURATION: {
            a_to_b_tag = ICC_TAG_A_TO_B2;
            break;
        }
    }

    ParseCtx lut_ctx;
    TRY(icc_tag_table_lookup(
        profile.tag_table,
        icc_tag_signature(a_to_b_tag),
        &lut_ctx
    ));

    uint32_t lut_signature;
    TRY(parse_ctx_read_u32_be(&lut_ctx, &lut_signature));

    switch (lut_signature) {
        case 0x6D667431: {
            ICCLut8 lut;
            TRY(icc_parse_lut8(lut_ctx, &lut));

            double lut_output[15];
            TRY(icc_lut8_map(lut, color, lut_output));
            *output = (ICCPcsColor) {
                .vec =
                    geom_vec3_new(lut_output[0], lut_output[1], lut_output[2]),
                .is_xyz = profile.header.pcs
                       == icc_color_space_signature(ICC_COLOR_SPACE_XYZ)
            };
            break;
        }
        case 0x6D667432: {
            ICCLut16 lut;
            TRY(icc_parse_lut16(lut_ctx, &lut));

            double lut_output[15];
            TRY(icc_lut16_map(lut, color, lut_output));
            *output = (ICCPcsColor) {
                .vec =
                    geom_vec3_new(lut_output[0], lut_output[1], lut_output[2]),
                .is_xyz = profile.header.pcs
                       == icc_color_space_signature(ICC_COLOR_SPACE_XYZ)
            };
            break;
        }
        default: {
            return ERROR(ICC_ERR_INVALID_LUT, "Unknown lut signature");
        }
    }

    return NULL;
}

Error* icc_pcs_to_device(
    ICCProfile profile,
    ICCRenderingIntent rendering_intent,
    ICCPcsColor color,
    ICCColor* output
) {
    RELEASE_ASSERT(output);

    if (color.is_xyz
        && ICC_COLOR_SPACE_XYZ
               != icc_color_space_from_signature(profile.header.pcs)) {
        return ERROR(
            ICC_ERR_INCORRECT_SPACE,
            "Input pcs (is_xyz=%d) doesn't match pcs %d",
            (int)color.is_xyz,
            (int)icc_color_space_from_signature(profile.header.pcs)
        );
    }

    ICCTag b_to_a_tag;
    switch (rendering_intent) {
        case ICC_INTENT_MEDIA_RELATIVE:
        case ICC_INTENT_ABSOLUTE: {
            b_to_a_tag = ICC_TAG_B_TO_A1;
            break;
        }
        case ICC_INTENT_PERCEPTUAL: {
            b_to_a_tag = ICC_TAG_B_TO_A0;
            break;
        }
        case ICC_INTENT_SATURATION: {
            b_to_a_tag = ICC_TAG_B_TO_A2;
            break;
        }
    }

    ParseCtx lut_ctx;
    TRY(icc_tag_table_lookup(
        profile.tag_table,
        icc_tag_signature(b_to_a_tag),
        &lut_ctx
    ));

    uint32_t lut_signature;
    TRY(parse_ctx_read_u32_be(&lut_ctx, &lut_signature));

    switch (lut_signature) {
        case 0x6D667431: {
            ICCLut8 lut;
            TRY(icc_parse_lut8(lut_ctx, &lut));
            TRY(icc_lut8_map(lut, icc_pcs_to_color(color), output->channels));
            output->color_space =
                icc_color_space_from_signature(profile.header.color_space);
            break;
        }
        case 0x6D667432: {
            ICCLut16 lut;
            TRY(icc_parse_lut16(lut_ctx, &lut));
            TRY(icc_lut16_map(lut, icc_pcs_to_color(color), output->channels));
            output->color_space =
                icc_color_space_from_signature(profile.header.color_space);
            break;
        }
        default: {
            return ERROR(
                ICC_ERR_INVALID_LUT,
                "Unknown lut signature 0x%lx",
                (unsigned long)lut_signature
            );
        }
    }

    return NULL;
}

Error* icc_pcs_to_pcs(
    ICCProfile src_profile,
    ICCProfile dst_profile,
    bool src_is_absolute,
    ICCRenderingIntent intent,
    ICCPcsColor src,
    ICCPcsColor* dst
) {
    RELEASE_ASSERT(dst);
    RELEASE_ASSERT(src.is_xyz == icc_profile_is_pcsxyz(src_profile));

    bool intermediate_is_xyz =
        src.is_xyz || intent == ICC_INTENT_ABSOLUTE || src_is_absolute;
    bool dst_is_xyz = dst_profile.header.pcs
                   == icc_color_space_signature(ICC_COLOR_SPACE_XYZ);

    ICCPcsColor intermediate;
    const CieXYZ D50 = {.x = 0.9642, .y = 1.0, .z = 0.8249};

    if (intermediate_is_xyz) {
        intermediate = icc_pcs_color_to_xyz(src);
    } else {
        intermediate = src;
    }

    if (src_is_absolute && intent == ICC_INTENT_ABSOLUTE) {
        // Absolute -> absolute
        ICCXYZNumber src_mw, dst_mw;
        TRY(icc_media_whitepoint(src_profile, &src_mw));
        TRY(icc_media_whitepoint(dst_profile, &dst_mw));
        intermediate.vec = geom_vec3_mul(
            intermediate.vec,
            geom_vec3_div(
                icc_xyz_number_to_geom(dst_mw),
                icc_xyz_number_to_geom(src_mw)
            )
        );
    } else if (intent == ICC_INTENT_ABSOLUTE) {
        // Relative -> absolute
        ICCXYZNumber dst_mw;
        TRY(icc_media_whitepoint(dst_profile, &dst_mw));
        intermediate.vec = geom_vec3_mul(
            intermediate.vec,
            geom_vec3_div(icc_xyz_number_to_geom(dst_mw), cie_xyz_to_geom(D50))
        );
    } else if (src_is_absolute) {
        // Absolute -> relative
        ICCXYZNumber src_mw;
        TRY(icc_media_whitepoint(src_profile, &src_mw));
        intermediate.vec = geom_vec3_mul(
            intermediate.vec,
            geom_vec3_div(cie_xyz_to_geom(D50), icc_xyz_number_to_geom(src_mw))
        );
    }

    if (dst_is_xyz) {
        *dst = icc_pcs_color_to_xyz(intermediate);
    } else {
        *dst = icc_pcs_color_to_lab(intermediate);
    }

    return NULL;
}
