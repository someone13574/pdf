#pragma once

#include "pdf/catalog.h"
#include "pdf/error.h"
#include "pdf/object.h"
#include "pdf/resolver.h"

typedef struct {
    PdfInteger size;
    PdfCatalogRef root;
    PdfObject* raw_dict;
} PdfTrailer;

PdfError* pdf_deserialize_trailer(
    PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    PdfTrailer* deserialized
);
