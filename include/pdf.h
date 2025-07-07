#pragma once

#include <stdlib.h>

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

PdfObject* pdf_get_trailer(PdfDocument* doc, PdfResult* result);
PdfObject* pdf_get_root(PdfDocument* doc, PdfResult* result);
PdfObject* pdf_get_ref(PdfDocument* doc, PdfObjectRef ref, PdfResult* result);
