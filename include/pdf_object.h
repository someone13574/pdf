#pragma once

#include <stdint.h>

#include "vec.h"

typedef struct PdfObject PdfObject;

typedef struct {
    PdfObject* key;
    PdfObject* value;
} PdfObjectDictEntry;

typedef struct {
    PdfObject* stream_dict;
    char* stream_bytes;
} PdfObjectStream;

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
    PDF_OBJECT_KIND_STREAM,
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
    Vec* array_data;
    Vec* dict_data;
    PdfObjectStream stream_data;
    PdfObjectIndirect indirect_data;
    PdfObjectIndirectRef ref_data;
} PdfObjectData;

struct PdfObject {
    PdfObjectKind kind;
    PdfObjectData data;
};

// Generates a pretty-printed PdfObject string. If no arena is passed, you must
// free the string manually.
char* pdf_fmt_object(Arena* arena, PdfObject* object);
