#pragma once

#include "arena.h"
#include "pdf_doc.h"
#include "pdf_object.h"
#include "pdf_result.h"

#define SCHEMA_DECLARE_REF(schema_type, schema_name)                             \
    typedef struct schema_type##Ref {                                            \
        schema_type* (*get)(struct schema_type##Ref*, PdfDocument*, PdfResult*); \
        PdfObjectRef ref;                                                        \
        schema_type* cached;                                                     \
    } schema_type##Ref;                                                          \
    schema_type* pdf_schema_##schema_name##_get_ref(                             \
        schema_type##Ref* ref,                                                   \
        PdfDocument* doc,                                                        \
        PdfResult* result                                                        \
    );

#define SCHEMA_DECLARE_OPTIONAL_REF(schema_type, schema_name)                            \
    typedef struct schema_type##OptionalRef {                                            \
        schema_type* (*get)(struct schema_type##OptionalRef*, PdfDocument*, PdfResult*); \
        PdfObjectRefOptional ref;                                                        \
        schema_type* cached;                                                             \
    } schema_type##OptionalRef;                                                          \
    schema_type* pdf_schema_##schema_name##_get_optional_ref(                            \
        schema_type##OptionalRef* schema_ref,                                            \
        PdfDocument* doc,                                                                \
        PdfResult* result                                                                \
    );

#define PDF_REF_GET(schema_ref, doc, result)                                   \
    schema_ref.get(&schema_ref, doc, result)

typedef struct PdfSchemaPageTreeNode PdfSchemaPageTreeNode;
SCHEMA_DECLARE_REF(PdfSchemaPageTreeNode, page_tree_node)
SCHEMA_DECLARE_OPTIONAL_REF(PdfSchemaPageTreeNode, page_tree_node)

typedef struct {
    PdfObjectName type;
    PdfSchemaPageTreeNodeRef parent;
    PdfObjectDict resources; // shouldn't be required, since it is inheritable
    PdfObject* dict;
} PdfSchemaPage;

struct PdfSchemaPageTreeNode {
    PdfObjectName type;
    PdfSchemaPageTreeNodeOptionalRef parent;
    PdfObjectArray kids;
    PdfObjectInteger count;
    PdfObject* dict;
};

PdfSchemaPageTreeNode*
pdf_schema_page_tree_node_new(Arena* arena, PdfObject* dict, PdfResult* result);

typedef struct {
    PdfObjectName type;
    PdfSchemaPageTreeNodeRef pages;
    PdfObject* dict;
} PdfSchemaCatalog;

PdfSchemaCatalog*
pdf_schema_catalog_new(Arena* arena, PdfObject* dict, PdfResult* result);

SCHEMA_DECLARE_REF(PdfSchemaCatalog, catalog)

typedef struct {
    PdfObjectInteger size;
    PdfSchemaCatalogRef root;
    PdfObject* dict;
} PdfSchemaTrailer;

PdfSchemaTrailer*
pdf_schema_trailer_new(Arena* arena, PdfObject* dict, PdfResult* result);
