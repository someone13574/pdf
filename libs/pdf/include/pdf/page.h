#pragma once

#include "pdf/deserde_types.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf/resources.h"
#include "pdf/types.h"

// Page tree node
typedef struct {
    PdfName type;
    PdfOpDict parent;
    PdfVoidVec* kids;
    PdfInteger count;

    const PdfObject* raw_dict;
} PdfPageTreeNode;

PdfError* pdf_deserialize_page_tree_node(
    const PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    PdfPageTreeNode* deserialized
);

DESERIALIZABLE_STRUCT_REF(PdfPageTreeNode, page_tree_node)

DESERIALIZABLE_ARRAY_TYPE(PdfContentsArray)
DESERIALIZABLE_OPTIONAL_TYPE(PdfOpContentsArray, PdfContentsArray)

typedef struct {
    PdfName type;
    PdfPageTreeNodeRef parent;
    PdfOpResources resources;
    PdfRectangle media_box;
    PdfOpContentsArray contents;
    PdfOpInteger rotate;
    PdfOpDict group;

    const PdfObject* raw_dict;
} PdfPage;

PdfError* pdf_deserialize_page(
    const PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    PdfPage* deserialized
);

DESERIALIZABLE_STRUCT_REF(PdfPage, page)
DESERIALIZABLE_ARRAY_TYPE(PdfPageArray)
