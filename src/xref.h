#pragma once

#include "ctx.h"
#include "result.h"

typedef struct XRefTable XRefTable;

XRefTable*
pdf_xref_new(Arena* arena, PdfCtx* ctx, size_t xrefstart, PdfResult* result);

PdfResult pdf_xref_get_ref_location(
    XRefTable* xref,
    size_t object_id,
    size_t generation,
    size_t* location
);
