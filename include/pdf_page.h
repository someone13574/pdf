#pragma once

#include "deserialize_types.h"
#include "pdf_object.h"
#include "pdf_resolver.h"

// Page tree node
typedef struct {
    PdfName type;
    PdfOpDict parent;
    Vec* kids;
    PdfInteger count;
    PdfObject* raw_dict;
} PdfPageTreeNode;

PdfResult pdf_deserialize_page_tree_node(
    PdfObject* object,
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
    PdfOpDict resources;
    PdfOpArray media_box;
    PdfOpContentsArray contents;
    PdfObject* raw_dict;
} PdfPage;

PdfResult pdf_deserialize_page(
    PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    PdfPage* deserialized
);

DESERIALIZABLE_STRUCT_REF(PdfPage, page)
DESERIALIZABLE_ARRAY_TYPE(PdfPageArray)
