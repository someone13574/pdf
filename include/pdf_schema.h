#pragma once

#define SCHEMA_NAME PdfSchemaPageTreeNode
#define SCHEMA_LOWERCASE_NAME pdf_schema_page_tree_node
#define SCHEMA_FIELDS                                                          \
    X(PdfObjectName, type)                                                     \
    X(PdfSchemaPageTreeNodeOpRef, parent)                                      \
    X(PdfObjectArray, kids) X(PdfObjectInteger, count)
#include "decl_schema.h"

#define SCHEMA_NAME PdfSchemaPage
#define SCHEMA_LOWERCASE_NAME pdf_schema_page
#define SCHEMA_FIELDS                                                          \
    X(PdfObjectName, type)                                                     \
    X(PdfSchemaPageTreeNodeRef, parent)                                        \
    X(PdfObjectDict, resources)
#include "decl_schema.h"

#define SCHEMA_NAME PdfSchemaCatalog
#define SCHEMA_LOWERCASE_NAME pdf_schema_catalog
#define SCHEMA_FIELDS X(PdfObjectName, type) X(PdfSchemaPageTreeNodeRef, pages)
#include "decl_schema.h"

#define SCHEMA_NAME PdfSchemaTrailer
#define SCHEMA_LOWERCASE_NAME pdf_schema_trailer
#define SCHEMA_FIELDS X(PdfObjectInteger, size) X(PdfSchemaCatalogRef, root)
#include "decl_schema.h"

#define PDF_RESOLVE(schema_ref, doc, result)                                   \
    schema_ref.resolve(&schema_ref, doc, result)
