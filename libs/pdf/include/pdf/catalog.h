#pragma once

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
    PdfCatalog* target_ptr,
    PdfOptionalResolver resolver,
    Arena* arena
);

DESERDE_DECL_RESOLVABLE(
    PdfCatalogRef,
    PdfCatalog,
    pdf_catalog_ref_init,
    pdf_resolve_catalog
)
