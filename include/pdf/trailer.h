#pragma once

#include "pdf/catalog.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf/result.h"

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
