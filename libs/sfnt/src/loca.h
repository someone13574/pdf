#pragma once

#include "arena/arena.h"
#include "err/error.h"
#include "maxp.h"
#include "parser.h"
#include "sfnt/sfnt.h"

typedef struct {
    Uint32Array* offsets;
} SfntLoca;

Error* sfnt_parse_loca(
    Arena* arena,
    SfntParser* parser,
    const SfntHead* head,
    const SfntMaxp* maxp,
    SfntLoca* loca
);

Error*
sfnt_loca_glyph_offset(const SfntLoca* loca, uint32_t gid, uint32_t* offset);
