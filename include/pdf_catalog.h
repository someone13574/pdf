#pragma once

#include "deserialize_types.h"
#include "pdf_doc.h"
#include "pdf_object.h"
#include "pdf_page.h"
#include "pdf_result.h"

// Catalog
typedef struct {
    PdfName type;
    PdfPageTreeNodeRef pages;
    PdfObject* raw_dict;
} PdfCatalog;

PdfCatalog*
pdf_deserialize_catalog(PdfObject* object, PdfDocument* doc, PdfResult* result);

DESERIALIZABLE_STRUCT_REF(PdfCatalog, catalog)
