#pragma once

#include "arena.h"
#include "pdf_catalog.h"
#include "pdf_object.h"
#include "pdf_result.h"

typedef struct {
    PdfInteger size;
    PdfCatalogRef root;
    PdfObject* raw_dict;
} PdfTrailer;

PdfTrailer*
pdf_deserialize_trailer(PdfObject* object, Arena* arena, PdfResult* result);
