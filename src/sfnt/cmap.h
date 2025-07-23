#pragma once

#include "arena/arena.h"
#include "parser.h"
#include "sfnt/cmap.h"

PdfResult sfnt_parse_cmap(Arena* arena, SfntParser* parser, SfntCmap* cmap);
PdfResult
sfnt_cmap_get_encoding(SfntCmap* cmap, SfntParser* parser, size_t idx);
