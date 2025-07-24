#pragma once

#include "arena/arena.h"
#include "parser.h"
#include "pdf/result.h"
#include "sfnt/glyph.h"

PdfResult sfnt_parse_glyph(Arena* arena, SfntParser* parser, SfntGlyph* glyph);
