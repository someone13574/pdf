#pragma once

#include "arena/arena.h"
#include "err/error.h"
#include "parse_ctx/ctx.h"
#include "types.h"

typedef struct {
    CffSIDArray* glyph_names;
} CffCharset;

Error* cff_parse_charset(
    ParseCtx* ctx,
    Arena* arena,
    uint16_t num_glyphs,
    CffCharset* charset_out
);
