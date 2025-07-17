#pragma once

#include "arena.h"
#include "pdf_object.h"
#include "pdf_result.h"

typedef struct PdfDocument PdfDocument;

PdfDocument* pdf_document_new(
    Arena* arena,
    char* buffer,
    size_t buffer_size,
    PdfResult* result
);

Arena* pdf_doc_arena(PdfDocument* doc);
PdfObject* pdf_resolve(PdfDocument* doc, PdfIndirectRef ref, PdfResult* result);
