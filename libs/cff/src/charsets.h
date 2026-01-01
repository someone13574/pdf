#pragma once

#include "arena/arena.h"
#include "err/error.h"
#include "parser.h"
#include "types.h"

#define DARRAY_NAME CffSIDArray
#define DARRAY_LOWERCASE_NAME cff_sid_array
#define DARRAY_TYPE CffSID
#include "arena/darray_decl.h"

typedef struct {
    CffSIDArray* glyph_names;
} CffCharset;

Error* cff_parse_charset(
    CffParser* parser,
    Arena* arena,
    CffCard16 num_glyphs,
    CffCharset* charset_out
);
