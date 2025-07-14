#include <stddef.h>

#include "arena.h"
#include "deserialize.h"
#include "pdf_catalog.h"
#include "pdf_object.h"
#include "pdf_result.h"

PdfPageTreeNode* pdf_deserialize_page_tree_node(
    PdfObject* object,
    Arena* arena,
    PdfResult* result
) {
    if (!object || !arena || !result) {
        return NULL;
    }

    PdfFieldDescriptor fields[] = {
        PDF_OBJECT_FIELD(PdfPageTreeNode, "Type", type, PDF_OBJECT_TYPE_NAME),
        PDF_OP_OBJECT_FIELD(
            PdfPageTreeNode,
            "Parent",
            parent,
            PDF_OBJECT_TYPE_DICT,
            PdfOpDict
        ),
        PDF_OBJECT_FIELD(PdfPageTreeNode, "Kids", kids, PDF_OBJECT_TYPE_ARRAY),
        PDF_OBJECT_FIELD(
            PdfPageTreeNode,
            "Count",
            count,
            PDF_OBJECT_TYPE_INTEGER
        )
    };

    PdfPageTreeNode page_tree_node = {.raw_dict = object};
    *result = pdf_deserialize_object(
        &page_tree_node,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        object
    );

    if (*result != PDF_OK) {
        return NULL;
    }

    PdfPageTreeNode* allocated = arena_alloc(arena, sizeof(PdfPageTreeNode));
    *allocated = page_tree_node;

    return allocated;
}

PDF_RESOLVE_IMPL(
    PdfPageTreeNode,
    pdf_resolve_page_tree_node,
    pdf_deserialize_page_tree_node
)

PdfCatalog*
pdf_deserialize_catalog(PdfObject* object, Arena* arena, PdfResult* result) {
    if (!object || !arena || !result) {
        return NULL;
    }

    PdfFieldDescriptor fields[] = {
        PDF_OBJECT_FIELD(PdfCatalog, "Type", type, PDF_OBJECT_TYPE_NAME),
        PDF_REF_FIELD(PdfCatalog, "Pages", pages, PdfPageTreeNodeRef)
    };

    PdfCatalog catalog = {.raw_dict = object};
    *result = pdf_deserialize_object(
        &catalog,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        object
    );

    if (*result != PDF_OK) {
        return NULL;
    }

    PdfCatalog* allocated = arena_alloc(arena, sizeof(PdfCatalog));
    *allocated = catalog;

    return allocated;
}

PDF_RESOLVE_IMPL(PdfCatalog, pdf_resolve_catalog, pdf_deserialize_catalog)
