#pragma once

#include "arena/arena.h"
#include "parser.h"
#include "pdf/error.h"
#include "sfnt/glyph.h"

PdfError* sfnt_parse_glyph(Arena* arena, SfntParser* parser, SfntGlyph* glyph);
