#pragma once

#include "deserialize_types.h"
#include "pdf/object.h"
#include "pdf/page.h"
#include "pdf/resolver.h"
#include "pdf/result.h"

// Catalog
typedef struct {
    PdfName type;
    PdfPageTreeNodeRef pages;
    PdfObject* raw_dict;
} PdfCatalog;

PdfResult pdf_deserialize_catalog(
    PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    PdfCatalog* deserialized
);

DESERIALIZABLE_STRUCT_REF(PdfCatalog, catalog)
