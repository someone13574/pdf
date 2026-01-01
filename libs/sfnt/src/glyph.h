#pragma once

#include "arena/arena.h"
#include "err/error.h"
#include "parser.h"
#include "sfnt/glyph.h"

Error* sfnt_parse_glyph(Arena* arena, SfntParser* parser, SfntGlyph* glyph);
