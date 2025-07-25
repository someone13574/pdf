#pragma once

#include "arena/arena.h"
#include "maxp.h"
#include "parser.h"
#include "pdf_error/error.h"
#include "sfnt/sfnt.h"

typedef struct {
    SfntUint32Array* offsets;
} SfntLoca;

PdfError* sfnt_parse_loca(
    Arena* arena,
    SfntParser* parser,
    SfntHead* head,
    SfntMaxp* maxp,
    SfntLoca* loca
);

PdfError*
sfnt_loca_glyph_offset(SfntLoca* loca, uint32_t gid, uint32_t* offset);
