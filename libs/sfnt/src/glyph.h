#pragma once

#include "arena/arena.h"
#include "err/error.h"
#include "parse_ctx/ctx.h"
#include "sfnt/glyph.h"

Error* sfnt_parse_glyph(Arena* arena, ParseCtx parser, SfntGlyph* glyph);
