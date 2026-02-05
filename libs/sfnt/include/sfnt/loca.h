#pragma once

#include "arena/arena.h"
#include "arena/common.h"
#include "err/error.h"
#include "parse_ctx/ctx.h"
#include "sfnt/head.h"
#include "sfnt/maxp.h"

typedef struct {
    Uint32Array* offsets;
} SfntLoca;

Error* sfnt_parse_loca(
    Arena* arena,
    ParseCtx ctx,
    const SfntHead* head,
    const SfntMaxp* maxp,
    SfntLoca* loca
);

Error*
sfnt_loca_glyph_offset(const SfntLoca* loca, uint32_t gid, uint32_t* offset);
