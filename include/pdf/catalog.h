#pragma once

#include "pdf/deserde_types.h"
#include "pdf/object.h"
#include "pdf/page.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"

// Catalog
typedef struct {
    PdfName type;
    PdfPageTreeNodeRef pages;
    const PdfObject* raw_dict;
} PdfCatalog;

PdfError* pdf_deserialize_catalog(
    const PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    PdfCatalog* deserialized
);

DESERIALIZABLE_STRUCT_REF(PdfCatalog, catalog)
