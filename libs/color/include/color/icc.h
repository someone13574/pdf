#pragma once

#include <stdint.h>

#include "arena/arena.h"
#include "color/icc_color.h"
#include "err/error.h"
#include "geom/vec3.h"
#include "parse_ctx/ctx.h"

typedef enum {
    ICC_INTENT_MEDIA_RELATIVE,
    ICC_INTENT_ABSOLUTE,
    ICC_INTENT_PERCEPTUAL,
    ICC_INTENT_SATURATION
} IccRenderingIntent;

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

typedef struct {
    uint32_t size;
    uint32_t preferred_cmm;
    uint32_t version;
    uint32_t class;
    uint32_t color_space;
    uint32_t pcs;
    IccDateTime date;
    uint32_t signature;
    uint32_t platform_signature;
    uint32_t flags;
    uint32_t device_manufacturer;
    uint32_t device_model;
    uint64_t device_attributes;
    uint32_t rendering_intent;
    IccXYZNumber illuminant;
    uint32_t creator_signature;
    uint64_t id_low;
    uint64_t id_high;
} IccProfileHeader;

Error* icc_parse_header(ParseCtx* ctx, IccProfileHeader* out);

typedef struct {
    ParseCtx file_ctx;
    ParseCtx table_ctx;
    uint32_t tag_count;
} IccTagTable;

Error* icc_tag_table_new(ParseCtx* file_ctx, IccTagTable* out);
Error*
icc_tag_table_lookup(IccTagTable table, uint32_t tag_signature, ParseCtx* out);

typedef struct {
    IccProfileHeader header;
    IccTagTable tag_table;
} IccProfile;

Error* icc_parse_profile(ParseCtx ctx, IccProfile* profile);
bool icc_profile_is_pcsxyz(IccProfile profile);

Error* icc_device_to_pcs(
    IccProfile profile,
    IccRenderingIntent rendering_intent,
    IccColor color,
    IccPcsColor* output
);

Error* icc_pcs_to_device(
    Arena* arena,
    IccProfile profile,
    IccRenderingIntent rendering_intent,
    IccPcsColor color,
    IccColor* output
);

Error* icc_pcs_to_pcs(
    IccProfile src_profile,
    IccProfile dst_profile,
    bool src_is_absolute,
    IccRenderingIntent intent,
    IccPcsColor src,
    IccPcsColor* dst
);
