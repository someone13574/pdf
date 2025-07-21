#pragma once

#include "deserialize_types.h"
#include "pdf_object.h"
#include "pdf_page.h"
#include "pdf_resolver.h"
#include "pdf_result.h"

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
