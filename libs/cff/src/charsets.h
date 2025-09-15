#pragma once

#include "arena/arena.h"
#include "parser.h"
#include "pdf_error/error.h"
#include "types.h"

#define DARRAY_NAME CffSIDArray
#define DARRAY_LOWERCASE_NAME cff_sid_array
#define DARRAY_TYPE CffSID
#include "arena/darray_decl.h"

typedef struct {
    CffSIDArray* glyph_names;
} CffCharset;

PdfError* cff_parse_charset(
    CffParser* parser,
    Arena* arena,
    CffCard16 num_glyphs,
    CffCharset* charset_out
);
