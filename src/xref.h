#pragma once

#include "ctx.h"
#include "pdf/error.h"
#include "pdf/object.h"

typedef struct XRefTable XRefTable;

typedef struct {
    size_t offset;
    size_t generation;
    bool entry_parsed;
    PdfObject* object;
} XRefEntry;

PdfError*
pdf_xref_new(Arena* arena, PdfCtx* ctx, size_t xrefstart, XRefTable** xref);

PdfError* pdf_xref_get_entry(
    XRefTable* xref,
    size_t object_id,
    size_t generation,
    XRefEntry** entry
);
