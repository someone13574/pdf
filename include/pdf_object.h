#pragma once

#include <stdint.h>

#include "pdf_result.h"
#include "vec.h"

typedef struct PdfObject PdfObject;

typedef bool PdfObjectBoolean;
typedef int32_t PdfObjectInteger;
typedef double PdfObjectReal;
typedef char* PdfObjectString;
typedef char* PdfObjectName;
typedef Vec* PdfObjectArray;
typedef Vec* PdfObjectDict;

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
} PdfObjectRef;

#define DECL_OPTIONAL_TYPE(type)                                               \
    typedef struct {                                                           \
        bool has_value;                                                        \
        type value;                                                            \
    } type##Optional;

DECL_OPTIONAL_TYPE(PdfObjectBoolean)
DECL_OPTIONAL_TYPE(PdfObjectInteger)
DECL_OPTIONAL_TYPE(PdfObjectReal)
DECL_OPTIONAL_TYPE(PdfObjectString)
DECL_OPTIONAL_TYPE(PdfObjectName)
DECL_OPTIONAL_TYPE(PdfObjectArray)
DECL_OPTIONAL_TYPE(PdfObjectDict)
DECL_OPTIONAL_TYPE(PdfObjectStream)
DECL_OPTIONAL_TYPE(PdfObjectIndirect)
DECL_OPTIONAL_TYPE(PdfObjectRef)

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
    PdfObjectBoolean boolean;
    PdfObjectInteger integer;
    PdfObjectReal real;
    PdfObjectString string;
    PdfObjectName name;
    PdfObjectArray array;
    PdfObjectDict dict;
    PdfObjectStream stream;
    PdfObjectIndirect indirect;
    PdfObjectRef ref;
} PdfObjectData;

struct PdfObject {
    PdfObjectKind kind;
    PdfObjectData data;
};

// Gets the value associated with a given key in a dictionary object
PdfObject*
pdf_object_dict_get(PdfObject* dict, const char* key, PdfResult* result);

// Generates a pretty-printed PdfObject string. If no arena is passed, you must
// free the string manually.
char* pdf_fmt_object(Arena* arena, PdfObject* object);
