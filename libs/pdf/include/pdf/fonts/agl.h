#pragma once

#include <stdint.h>

#include "arena/arena.h"
#include "pdf_error/error.h"

typedef struct PdfAglGlyphList PdfAglGlyphList;

PdfAglGlyphList* pdf_parse_agl_glyphlist(Arena* arena, const char* code);
PdfError* pdf_agl_glyphlist_lookup(
    PdfAglGlyphList* glyphlist,
    const char* glyph_name,
    uint16_t* codepoints_out,
    uint8_t* num_codepoints_out
);
