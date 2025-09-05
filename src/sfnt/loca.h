#pragma once

#include "arena/arena.h"
#include "maxp.h"
#include "parser.h"
#include "pdf_error/error.h"
#include "sfnt/sfnt.h"

typedef struct {
    Uint32Array* offsets;
} SfntLoca;

PdfError* sfnt_parse_loca(
    Arena* arena,
    SfntParser* parser,
    const SfntHead* head,
    const SfntMaxp* maxp,
    SfntLoca* loca
);

PdfError*
sfnt_loca_glyph_offset(const SfntLoca* loca, uint32_t gid, uint32_t* offset);
