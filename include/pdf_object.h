#pragma once

#include <stdint.h>

#include "pdf_result.h"
#include "vec.h"

typedef struct PdfObject PdfObject;

typedef bool PdfObjectBoolean;
typedef uint32_t PdfObjectInteger;
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

typedef struct {
    bool has_value;
    PdfObjectBoolean value;
} PdfObjectBooleanOptional;

typedef struct {
    bool has_value;
    PdfObjectInteger value;
} PdfObjectIntegerOptional;

typedef struct {
    bool has_value;
    PdfObjectReal value;
} PdfObjectRealOptional;

typedef struct {
    bool has_value;
    PdfObjectString value;
} PdfObjectStringOptional;

typedef struct {
    bool has_value;
    PdfObjectName value;
} PdfObjectNameOptional;

typedef struct {
    bool has_value;
    PdfObjectArray value;
} PdfObjectArrayOptional;

typedef struct {
    bool has_value;
    PdfObjectDict value;
} PdfObjectDictOptional;

typedef struct {
    bool has_value;
    PdfObjectStream value;
} PdfObjectStreamOptional;

typedef struct {
    bool has_value;
    PdfObjectIndirect value;
} PdfObjectIndirectOptional;

typedef struct {
    bool has_value;
    PdfObjectRef value;
} PdfObjectRefOptional;

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
