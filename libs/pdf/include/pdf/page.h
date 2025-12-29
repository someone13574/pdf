#pragma once

#include "pdf/content_stream/stream.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf/resources.h"

typedef struct PdfPageTreeNode PdfPageTreeNode;
DESERDE_DECL_RESOLVABLE(
    PdfPageTreeNodeRef,
    PdfPageTreeNode,
    pdf_page_tree_node_ref_init,
    pdf_resolve_page_tree_node
)

typedef struct {
    PdfName type;
    PdfPageTreeNodeRef parent;
    PdfResourcesOptional resources;
    PdfRectangle media_box;
    PdfRectangleOptional crop_box;
    PdfContentsStreamRefVecOptional contents;
    PdfIntegerOptional rotate;
    PdfDictOptional group;
} PdfPage;

PdfError* pdf_deserialize_page(
    const PdfObject* object,
    PdfPage* target_ptr,
    PdfResolver* resolver
);

DESERDE_DECL_RESOLVABLE(
    PdfPageRef,
    PdfPage,
    pdf_page_ref_init,
    pdf_resolve_page
)

#define DVEC_NAME PdfPageRefVec
#define DVEC_LOWERCASE_NAME pdf_page_ref_vec
#define DVEC_TYPE PdfPageRef
#include "arena/dvec_decl.h"

struct PdfPageTreeNode {
    PdfName type;
    PdfDictOptional parent;
    PdfPageRefVec*
        kids; // TODO: Proper tree (support page tree node nodes, inheritance)
    PdfInteger count;
};

PdfError* pdf_deserialize_page_tree_node(
    const PdfObject* object,
    PdfPageTreeNode* target_ptr,
    PdfResolver* resolver
);
