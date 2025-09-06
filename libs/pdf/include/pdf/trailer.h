#pragma once

#include "pdf/catalog.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"

typedef struct {
    PdfInteger size;
    PdfCatalogRef root;

    const PdfObject* raw_dict;
} PdfTrailer;

PdfError* pdf_deserialize_trailer(
    const PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    PdfTrailer* deserialized
);
