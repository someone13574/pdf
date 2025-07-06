#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "arena.h"
#include "ctx.h"
#include "result.h"
#include "vec.h"

typedef struct PdfObject PdfObject;

typedef struct {
    Vec* elements;
} PdfObjectArray;

typedef struct {
    Vec** buckets;
} PdfObjectDict;

typedef struct {
    size_t object_id;
    size_t generation;
    PdfObject* object;
} PdfObjectIndirect;

typedef struct {
    size_t object_id;
    size_t generation;
} PdfObjectIndirectRef;

typedef enum {
    PDF_OBJECT_KIND_BOOLEAN,
    PDF_OBJECT_KIND_INTEGER,
    PDF_OBJECT_KIND_REAL,
    PDF_OBJECT_KIND_STRING,
    PDF_OBJECT_KIND_NAME,
    PDF_OBJECT_KIND_ARRAY,
    PDF_OBJECT_KIND_DICT,
    PDF_OBJECT_KIND_INDIRECT,
    PDF_OBJECT_KIND_REF,
    PDF_OBJECT_KIND_NULL
} PdfObjectKind;

typedef union {
    bool bool_data;
    int32_t integer_data;
    double real_data;
    char* string_data;
    char* name_data;
    PdfObjectArray array_data;
    PdfObjectDict dict_data;
    PdfObjectIndirect indirect_data;
    PdfObjectIndirectRef ref_data;

} PdfObjectData;

struct PdfObject {
    PdfObjectKind kind;
    PdfObjectData data;
};

PdfObject* pdf_parse_object(Arena* arena, PdfCtx* ctx, PdfResult* result);
