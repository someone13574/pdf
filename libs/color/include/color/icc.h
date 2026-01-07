#pragma once

#include <stdint.h>

#include "arena/arena.h"
#include "color/icc_color.h"
#include "color/icc_lut.h"
#include "color/icc_types.h"
#include "err/error.h"
#include "parse_ctx/ctx.h"

typedef enum {
    ICC_INTENT_MEDIA_RELATIVE,
    ICC_INTENT_ABSOLUTE,
    ICC_INTENT_PERCEPTUAL,
    ICC_INTENT_SATURATION
} IccRenderingIntent;

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
    Arena* arena;
    IccProfileHeader header;
    IccTagTable tag_table;

    bool cached_b2a0;
    bool cached_b2a1;
    bool cached_b2a2;
    IccLutBToA b2a0;
    IccLutBToA b2a1;
    IccLutBToA b2a2;
} IccProfile;

Error* icc_parse_profile(ParseCtx ctx, Arena* arena, IccProfile* out);
bool icc_profile_is_pcsxyz(IccProfile profile);

Error* icc_device_to_pcs(
    IccProfile profile,
    IccRenderingIntent rendering_intent,
    IccColor color,
    IccPcsColor* output
);

Error* icc_pcs_to_device(
    IccProfile* profile,
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

Error* icc_device_to_device(
    IccProfile* src_profile,
    IccProfile* dst_profile,
    IccRenderingIntent intent,
    IccColor src,
    IccColor* dst
);
