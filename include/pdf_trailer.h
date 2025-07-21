#pragma once

#include "pdf_catalog.h"
#include "pdf_object.h"
#include "pdf_resolver.h"
#include "pdf_result.h"

typedef struct {
    PdfInteger size;
    PdfCatalogRef root;
    PdfObject* raw_dict;
} PdfTrailer;

PdfResult pdf_deserialize_trailer(
    PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    PdfTrailer* deserialized
);
