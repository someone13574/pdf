#pragma once

#include "arena.h"
#include "pdf_object.h"
#include "pdf_result.h"

typedef struct {
    PdfObjectInteger size;
    PdfObjectIntegerOptional prev;
    PdfObjectRef root;
    PdfObjectDictOptional encrypt;
    PdfObjectDictOptional info;
    PdfObjectArrayOptional id;
} PdfSchemaTrailer;

typedef struct {
    PdfObjectName type;
    PdfObjectDict pages;
} PdfSchemaCatalog;

PdfSchemaTrailer*
pdf_schema_trailer_new(Arena* arena, PdfObject* dict, PdfResult* result);
