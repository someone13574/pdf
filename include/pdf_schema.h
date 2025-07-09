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

#define PDF_REF_GET(schema_ref, doc, result)                                   \
    schema_ref.get(&schema_ref, doc, result)

typedef struct {
    PdfObjectName type;
    PdfObjectRef pages;
    PdfObject* dict;
} PdfSchemaCatalog;

SCHEMA_DECLARE_REF(PdfSchemaCatalog, catalog)

typedef struct {
    PdfObjectInteger size;
    PdfSchemaCatalogRef root;
    PdfObject* dict;
} PdfSchemaTrailer;

PdfSchemaTrailer*
pdf_schema_trailer_new(Arena* arena, PdfObject* dict, PdfResult* result);
