#pragma once

#include "arena/arena.h"
#include "maxp.h"
#include "parser.h"
#include "pdf/result.h"
#include "sfnt/sfnt.h"

typedef struct {
    SfntUint32Array* offsets;
} SfntLoca;

PdfResult sfnt_parse_loca(
    Arena* arena,
    SfntParser* parser,
    SfntHead* head,
    SfntMaxp* maxp,
    SfntLoca* loca
);

PdfResult
sfnt_loca_glyph_offset(SfntLoca* loca, uint32_t gid, uint32_t* offset);
