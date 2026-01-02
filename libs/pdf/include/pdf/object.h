#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "err/error.h"
#include "pdf/resolver.h"

typedef struct PdfObject PdfObject;

#define DVEC_NAME PdfObjectVec
#define DVEC_LOWERCASE_NAME pdf_object_vec
#define DVEC_TYPE PdfObject
#include "arena/dvec_decl.h"

typedef bool PdfBoolean;
typedef int32_t PdfInteger;
typedef double PdfReal;
typedef char* PdfName;

typedef struct {
    uint8_t* data;
    size_t len;
} PdfString;

#define DVEC_NAME PdfNameVec
#define DVEC_LOWERCASE_NAME pdf_name_vec
#define DVEC_TYPE PdfName
#include "arena/dvec_decl.h"

typedef struct {
    PdfObjectVec* elements;
} PdfArray;

typedef struct PdfDictEntry PdfDictEntry;

#define DVEC_NAME PdfDictEntryVec
#define DVEC_LOWERCASE_NAME pdf_dict_entry_vec
#define DVEC_TYPE PdfDictEntry
#include "arena/dvec_decl.h"

typedef struct {
    PdfDictEntryVec* entries;
} PdfDict;

typedef struct PdfStreamDict PdfStreamDict;

typedef struct {
    PdfStreamDict* stream_dict;

    const uint8_t* stream_bytes;
    size_t decoded_stream_len;
} PdfStream;

typedef struct {
    size_t object_id;
    size_t generation;
    PdfObject* object;
} PdfIndirectObject;

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
Error*
pdf_object_dict_get(const PdfObject* dict, const char* key, PdfObject* object);

// Generates a pretty-printed PdfObject string.
char* pdf_fmt_object(Arena* arena, const PdfObject* object);

struct PdfDictEntry {
    PdfName key;
    PdfObject value;
};
