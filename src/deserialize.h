#pragma once

#include <stddef.h>

#include "pdf_object.h"
#include "pdf_result.h"

typedef enum {
    PDF_FIELD_KIND_OBJECT,
    PDF_FIELD_KIND_OP_OBJECT,
    PDF_FIELD_KIND_REF,
    PDF_FIELD_KIND_OP_REF
} PdfFieldKind;

typedef struct {
    PdfObjectType type;
} PdfFieldDataObject;

typedef struct {
    PdfObjectType type;
    size_t discriminant_offset;
    size_t value_offset;
} PdfFieldDataOpObject;

typedef struct {
    size_t resolved_offset;
    size_t ref_offset;
} PdfFieldDataRef;

typedef struct {
    PdfFieldDataRef ref_data;
    size_t discriminant_offset;
    size_t data_offset;
} PdfFieldDataOpRef;

typedef union {
    PdfFieldDataObject object;
    PdfFieldDataOpObject op_object;
    PdfFieldDataRef ref;
    PdfFieldDataOpRef op_ref;
} PdfFieldData;

typedef struct {
    const char* key;
    size_t offset;
    PdfFieldKind kind;
    PdfFieldData data;
} PdfFieldDescriptor;

PdfResult pdf_deserialize_object(
    void* target,
    PdfFieldDescriptor* fields,
    size_t num_fields,
    PdfObject* object
);

#define PDF_RESOLVE_IMPL(struct_type, resolve_function, deserialize_function)  \
    struct_type* resolve_function(                                             \
        struct_type##Ref* ref,                                                 \
        PdfDocument* doc,                                                      \
        PdfResult* result                                                      \
    ) {                                                                        \
        if (!ref || !doc || !result) {                                         \
            return NULL;                                                       \
        }                                                                      \
        *result = PDF_OK;                                                      \
        if (ref->resolved) {                                                   \
            return ref->resolved;                                              \
        }                                                                      \
        PdfObject* object = pdf_get_ref(doc, ref->ref, result);                \
        if (*result != PDF_OK || !object) {                                    \
            return NULL;                                                       \
        }                                                                      \
        ref->resolved =                                                        \
            deserialize_function(object, pdf_doc_arena(doc), result);          \
        return ref->resolved;                                                  \
    }

#define PDF_OBJECT_FIELD(struct_type, key, field_name, object_type)            \
    {                                                                          \
        key, offsetof(struct_type, field_name), PDF_FIELD_KIND_OBJECT, {       \
            .object = {.type = (object_type) }                                 \
        }                                                                      \
    }

#define PDF_OP_OBJECT_FIELD(                                                   \
    struct_type,                                                               \
    key,                                                                       \
    field_name,                                                                \
    object_type_enum,                                                          \
    object_type                                                                \
)                                                                              \
    {                                                                          \
        key, offsetof(struct_type, field_name), PDF_FIELD_KIND_OP_OBJECT, {    \
            .op_object = {                                                     \
                .type = (object_type_enum),                                    \
                .discriminant_offset = offsetof(object_type, discriminant),    \
                .value_offset = offsetof(object_type, value)                   \
            }                                                                  \
        }                                                                      \
    }

#define PDF_REF_FIELD(struct_type, key, field_name, ref_type)                  \
    {                                                                          \
        key, offsetof(struct_type, field_name), PDF_FIELD_KIND_REF, {          \
            .ref = {                                                           \
                .resolved_offset = offsetof(ref_type, resolved),               \
                .ref_offset = offsetof(ref_type, ref)                          \
            }                                                                  \
        }                                                                      \
    }
