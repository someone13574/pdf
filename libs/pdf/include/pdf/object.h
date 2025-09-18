#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "pdf/resolver.h"
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

// TODO: Can this somehow have non-pointer elements? May need to add a variable
// so that the dvec doesn't forward declare.
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

#define DESERDE_DECL_OPTIONAL(new_type, base_type, init_fn)                    \
    typedef struct {                                                           \
        _Bool has_value;                                                       \
        base_type value;                                                       \
    } new_type;                                                                \
    void* init_fn(void* ptr_to_optional, _Bool has_value);

#define DESERDE_DECL_RESOLVABLE(new_type, base_type, init_fn, resolve_fn)      \
    typedef struct {                                                           \
        PdfIndirectRef ref;                                                    \
        base_type* resolved;                                                   \
    } new_type;                                                                \
    void init_fn(void* ptr_to_resolvable, PdfIndirectRef ref);                 \
    PdfError* resolve_fn(                                                      \
        new_type resolvable,                                                   \
        PdfResolver* resolver,                                                 \
        base_type* resolved_out                                                \
    );

#define DESERDE_DECL_TRAMPOLINE(trampoline_name)                               \
    PdfError* trampoline_name(                                                 \
        const PdfObject* object,                                               \
        void* target_ptr,                                                      \
        PdfOptionalResolver resolver,                                          \
        Arena* arena                                                           \
    );

/// Placeholder type for unimplemented deserialization fields. Choice is
/// arbitary.
typedef int PdfUnimplemented;

typedef struct {
    enum { PDF_NUMBER_TYPE_INTEGER, PDF_NUMBER_TYPE_REAL } type;

    union {
        PdfInteger integer;
        PdfReal real;
    } value;
} PdfNumber;

DESERDE_DECL_OPTIONAL(PdfNumberOptional, PdfNumber, pdf_number_op_init)

PdfError* pdf_deserialize_number(
    const PdfObject* object,
    PdfNumber* target_ptr,
    PdfOptionalResolver resolver,
    Arena* arena
);
DESERDE_DECL_TRAMPOLINE(pdf_deserialize_number_trampoline)

PdfReal pdf_number_as_real(PdfNumber number);

typedef struct {
    PdfNumber lower_left_x;
    PdfNumber lower_left_y;
    PdfNumber upper_right_x;
    PdfNumber upper_right_y;
} PdfRectangle;

DESERDE_DECL_OPTIONAL(PdfRectangleOptional, PdfRectangle, pdf_rectangle_op_init)

PdfError* pdf_deserialize_rectangle(
    const PdfObject* object,
    PdfRectangle* target_ptr,
    PdfOptionalResolver resolver,
    Arena* arena
);

DESERDE_DECL_TRAMPOLINE(pdf_deserialize_rectangle_trampoline)

#define DVEC_NAME PdfNameVec
#define DVEC_LOWERCASE_NAME pdf_name_vec
#define DVEC_TYPE PdfName
#include "arena/dvec_decl.h"
DESERDE_DECL_OPTIONAL(PdfNameVecOptional, PdfNameVec*, pdf_name_vec_op_init)

DESERDE_DECL_OPTIONAL(PdfBooleanOptional, PdfBoolean, pdf_boolean_op_init)
DESERDE_DECL_OPTIONAL(PdfIntegerOptional, PdfInteger, pdf_integer_op_init)
DESERDE_DECL_OPTIONAL(PdfRealOptional, PdfReal, pdf_real_op_init)
DESERDE_DECL_OPTIONAL(PdfStringOptional, PdfString, pdf_string_op_init)
DESERDE_DECL_OPTIONAL(PdfNameOptional, PdfName, pdf_name_op_init)
DESERDE_DECL_OPTIONAL(PdfArrayOptional, PdfArray, pdf_array_op_init)
DESERDE_DECL_OPTIONAL(PdfDictOptional, PdfDict, pdf_dict_op_init)
DESERDE_DECL_OPTIONAL(PdfStreamOptional, PdfStream, pdf_stream_op_init)
DESERDE_DECL_OPTIONAL(
    PdfIndirectObjectOptional,
    PdfIndirectObject,
    pdf_indirect_object_op_init
)
DESERDE_DECL_OPTIONAL(
    PdfIndirectRefOptional,
    PdfIndirectRef,
    pdf_indirect_ref_op_init
)

// TODO: unfuck the include hierarchy
struct PdfStreamDict {
    PdfInteger length;
    PdfNameVecOptional filter;

    // Additional entries in an embedded font stream dictionary. TODO: Since
    // unknown fields are now allowed, this should be a different structure.
    PdfIntegerOptional length1;
    PdfIntegerOptional length2;
    PdfIntegerOptional length3;
    PdfNameOptional subtype;
    PdfStreamOptional metadata;

    const PdfObject* raw_dict;
};

PdfError* pdf_deserialize_stream_dict(
    const PdfObject* object,
    PdfStreamDict* deserialized,
    Arena* arena
);

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
