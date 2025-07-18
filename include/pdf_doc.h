#pragma once

#include "arena.h"
#include "pdf_object.h"
#include "pdf_result.h"

typedef struct PdfDocument PdfDocument;

PdfResult pdf_document_new(
    Arena* arena,
    char* buffer,
    size_t buffer_size,
    PdfDocument** doc
);

Arena* pdf_doc_arena(PdfDocument* doc);
PdfResult
pdf_resolve(PdfDocument* doc, PdfIndirectRef ref, PdfObject* resolved);
