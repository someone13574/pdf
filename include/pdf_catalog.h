#pragma once

#include "arena.h"
#include "pdf_doc.h"
#include "pdf_object.h"
#include "pdf_result.h"

// Page Tree Node
typedef struct {
    PdfName type;
    PdfOpDict parent;
    PdfArray kids;
    PdfInteger count;
    PdfObject* raw_dict;
} PdfPageTreeNode;

PdfPageTreeNode* pdf_deserialize_page_tree_node(
    PdfObject* object,
    Arena* arena,
    PdfResult* result
);

typedef struct {
    PdfIndirectRef ref;
    void* resolved;
} PdfPageTreeNodeRef;

PdfPageTreeNode* pdf_resolve_page_tree_node(
    PdfPageTreeNodeRef* ref,
    PdfDocument* doc,
    PdfResult* result
);

// Catalog
typedef struct {
    PdfName type;
    PdfPageTreeNodeRef pages;
    PdfObject* raw_dict;
} PdfCatalog;

PdfCatalog*
pdf_deserialize_catalog(PdfObject* object, Arena* arena, PdfResult* result);

typedef struct {
    PdfIndirectRef ref;
    void* resolved;
} PdfCatalogRef;

PdfCatalog*
pdf_resolve_catalog(PdfCatalogRef* ref, PdfDocument* doc, PdfResult* result);
