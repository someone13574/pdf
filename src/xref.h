#pragma once

#include "ctx.h"
#include "pdf_object.h"
#include "pdf_result.h"

typedef struct {
    Arena* arena;
    PdfCtx* ctx;
    Vec* subsections;
} XRefTable;

typedef struct {
    size_t offset;
    size_t generation;
    bool entry_parsed;
    PdfObject* object;
} XRefEntry;

PdfResult
pdf_xref_new(Arena* arena, PdfCtx* ctx, size_t xrefstart, XRefTable* xref);

PdfResult pdf_xref_get_entry(
    XRefTable* xref,
    size_t object_id,
    size_t generation,
    XRefEntry** entry
);
