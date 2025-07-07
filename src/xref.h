#pragma once

#include "ctx.h"
#include "pdf_object.h"
#include "pdf_result.h"

typedef struct XRefTable XRefTable;

typedef struct {
    size_t offset;
    size_t generation;
    bool entry_parsed;
    PdfObject* object;
} XRefEntry;

XRefTable*
pdf_xref_new(Arena* arena, PdfCtx* ctx, size_t xrefstart, PdfResult* result);

XRefEntry* pdf_xref_get_entry(
    XRefTable* xref,
    size_t object_id,
    size_t generation,
    PdfResult* result
);
