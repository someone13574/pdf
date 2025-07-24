#pragma once

#include "arena/arena.h"
#include "maxp.h"
#include "parser.h"
#include "pdf/result.h"
#include "sfnt/sfnt.h"

typedef struct {
    SfntUint32Array* offsets;
} SfntLocal;

PdfResult sfnt_parse_local(
    Arena* arena,
    SfntParser* parser,
    SfntHead* head,
    SfntMaxp* maxp,
    SfntLocal* local
);

PdfResult
sfnt_local_glyph_offset(SfntLocal* local, uint32_t gid, uint32_t* offset);
