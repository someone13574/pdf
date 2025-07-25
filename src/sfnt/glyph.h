#pragma once

#include "arena/arena.h"
#include "parser.h"
#include "pdf_error/error.h"
#include "sfnt/glyph.h"

PdfError* sfnt_parse_glyph(Arena* arena, SfntParser* parser, SfntGlyph* glyph);
