#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "pdf_error/error.h"

typedef struct PdfObject PdfObject;

typedef bool PdfBoolean;
typedef int32_t PdfInteger;
typedef double PdfReal;
typedef char* PdfName;

typedef struct {
    uint8_t* data;
    size_t len;
} PdfString;

#define DVEC_NAME PdfObjectVec
#define DVEC_LOWERCASE_NAME pdf_object_vec
#define DVEC_TYPE PdfObject*
#include "arena/dvec_decl.h"

typedef struct {
    PdfObjectVec* elements;
} PdfArray;

typedef struct {
    // TODO: Replace key type with PdfName
    PdfObject* key;
    PdfObject* value;
} PdfDictEntry;

#define DVEC_NAME PdfDictEntryVec
#define DVEC_LOWERCASE_NAME pdf_dict_entry_vec
#define DVEC_TYPE PdfDictEntry
#include "arena/dvec_decl.h"

typedef struct {
    PdfDictEntryVec* entries;
} PdfDict;

// TODO: remove duplicate of `pdf_object_dict_get`
PdfObject* pdf_dict_get(const PdfDict* dict, PdfName key);

typedef struct PdfStreamDict PdfStreamDict;

typedef struct {
    PdfStreamDict* stream_dict;

    uint8_t* stream_bytes;
    size_t decoded_stream_len;
} PdfStream;

typedef struct {
    size_t object_id;
    size_t generation;
    PdfObject* object;
} PdfIndirectObject;

typedef struct {
    size_t object_id;
    size_t generation;
} PdfIndirectRef;

#define DECL_OPTIONAL_TYPE(type)                                               \
    typedef struct {                                                           \
        bool discriminant;                                                     \
        Pdf##type value;                                                       \
    } PdfOp##type;

DECL_OPTIONAL_TYPE(Boolean)
DECL_OPTIONAL_TYPE(Integer)
DECL_OPTIONAL_TYPE(Real)
DECL_OPTIONAL_TYPE(String)
DECL_OPTIONAL_TYPE(Name)
DECL_OPTIONAL_TYPE(Array)
DECL_OPTIONAL_TYPE(Dict)
DECL_OPTIONAL_TYPE(Stream)
DECL_OPTIONAL_TYPE(IndirectObject)
DECL_OPTIONAL_TYPE(IndirectRef)

#undef DECL_OPTIONAL_TYPE

typedef enum {
    PDF_OBJECT_TYPE_BOOLEAN,
    PDF_OBJECT_TYPE_INTEGER,
    PDF_OBJECT_TYPE_REAL,
    PDF_OBJECT_TYPE_STRING,
    PDF_OBJECT_TYPE_NAME,
    PDF_OBJECT_TYPE_ARRAY,
    PDF_OBJECT_TYPE_DICT,
    PDF_OBJECT_TYPE_STREAM,
    PDF_OBJECT_TYPE_INDIRECT_OBJECT,
    PDF_OBJECT_TYPE_INDIRECT_REF,
    PDF_OBJECT_TYPE_NULL
} PdfObjectType;

typedef union {
    PdfBoolean boolean;
    PdfInteger integer;
    PdfReal real;
    PdfString string;
    PdfName name;
    PdfArray array;
    PdfDict dict;
    PdfStream stream;
    PdfIndirectObject indirect_object;
    PdfIndirectRef indirect_ref;
} PdfObjectData;

struct PdfObject {
    PdfObjectType type;
    PdfObjectData data;
};

// Gets the value associated with a given key in a dictionary object
PdfError*
pdf_object_dict_get(const PdfObject* dict, const char* key, PdfObject* object);

// Generates a pretty-printed PdfObject string. If no arena is passed, you must
// free the string manually.
char* pdf_fmt_object(Arena* arena, const PdfObject* object);
