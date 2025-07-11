#include "pdf_schema.h"

// Page Tree Node
#define SCHEMA_NAME PdfSchemaPageTreeNode
#define SCHEMA_LOWERCASE_NAME pdf_schema_page_tree_node
#define SCHEMA_FIELDS                                                          \
    X("Type", type, REQUIRED, SCHEMA_FC_OBJECT, PDF_OBJECT_KIND_NAME, name)    \
    X("Parent",                                                                \
      parent,                                                                  \
      OPTIONAL,                                                                \
      SCHEMA_FC_SCHEMA_REF,                                                    \
      ,                                                                        \
      pdf_schema_page_tree_node_resolve)                                       \
    X("Kids", kids, REQUIRED, SCHEMA_FC_OBJECT, PDF_OBJECT_KIND_ARRAY, array)  \
    X("Count",                                                                 \
      count,                                                                   \
      REQUIRED,                                                                \
      SCHEMA_FC_OBJECT,                                                        \
      PDF_OBJECT_KIND_INTEGER,                                                 \
      integer)
#define SCHEMA_VALIDATION_FN schema_validation_none
#include "impl_schema.h"

// Page
#define SCHEMA_NAME PdfSchemaPage
#define SCHEMA_LOWERCASE_NAME pdf_schema_page
#define SCHEMA_FIELDS                                                          \
    X("Type", type, REQUIRED, SCHEMA_FC_OBJECT, PDF_OBJECT_KIND_NAME, name)    \
    X("Parent",                                                                \
      parent,                                                                  \
      REQUIRED,                                                                \
      SCHEMA_FC_SCHEMA_REF,                                                    \
      ,                                                                        \
      pdf_schema_page_tree_node_resolve)                                       \
    X("Resources",                                                             \
      resources,                                                               \
      REQUIRED,                                                                \
      SCHEMA_FC_OBJECT,                                                        \
      PDF_OBJECT_KIND_DICT,                                                    \
      dict)
#define SCHEMA_VALIDATION_FN schema_validation_none
#include "impl_schema.h"

// Catalog
#define SCHEMA_NAME PdfSchemaCatalog
#define SCHEMA_LOWERCASE_NAME pdf_schema_catalog
#define SCHEMA_FIELDS                                                          \
    X("Type", type, REQUIRED, SCHEMA_FC_OBJECT, PDF_OBJECT_KIND_NAME, name)    \
    X("Pages",                                                                 \
      pages,                                                                   \
      REQUIRED,                                                                \
      SCHEMA_FC_SCHEMA_REF,                                                    \
      ,                                                                        \
      pdf_schema_page_tree_node_resolve)
#define SCHEMA_VALIDATION_FN schema_validation_none
#include "impl_schema.h"

// Trailer
#define SCHEMA_NAME PdfSchemaTrailer
#define SCHEMA_LOWERCASE_NAME pdf_schema_trailer
#define SCHEMA_FIELDS                                                          \
    X("Size",                                                                  \
      size,                                                                    \
      REQUIRED,                                                                \
      SCHEMA_FC_OBJECT,                                                        \
      PDF_OBJECT_KIND_INTEGER,                                                 \
      integer)                                                                 \
    X("Root",                                                                  \
      root,                                                                    \
      REQUIRED,                                                                \
      SCHEMA_FC_SCHEMA_REF,                                                    \
      ,                                                                        \
      pdf_schema_catalog_resolve)
#define SCHEMA_VALIDATION_FN schema_validation_none
#include "impl_schema.h"
