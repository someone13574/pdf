#pragma once

#include <stddef.h>

#include "log.h"
#include "pdf_doc.h"
#include "pdf_object.h"
#include "pdf_result.h"

typedef enum {
    PDF_FIELD_KIND_OBJECT,
    PDF_FIELD_KIND_REF,
    PDF_FIELD_KIND_ARRAY,
    PDF_FIELD_KIND_AS_ARRAY,
    PDF_FIELD_KIND_OPTIONAL,
    PDF_FIELD_KIND_IGNORED
} PdfFieldKind;

typedef struct PdfFieldInfo PdfFieldInfo;

typedef struct {
    PdfObjectType type;
} PdfObjectFieldData;

typedef struct {
    size_t object_ref_offset;
} PdfRefFieldData;

typedef struct {
    size_t vec_offset;
    size_t element_size;
    PdfFieldInfo* element_info;
} PdfArrayFieldData;

typedef struct {
    size_t discriminant_offset;
    size_t data_offset;
    PdfFieldInfo* inner_info;
} PdfOptionalFieldData;

struct PdfFieldInfo {
    PdfFieldKind kind;

    union {
        PdfObjectFieldData object;
        PdfRefFieldData ref;
        PdfArrayFieldData array;
        PdfOptionalFieldData optional;
    } data;
};

typedef struct {
    const char* file;
    unsigned long line;
} PdfFieldDebugInfo;

typedef struct {
    const char* key;
    size_t offset;
    PdfFieldInfo info;
    PdfFieldDebugInfo debug;
} PdfFieldDescriptor;

PdfResult pdf_deserialize_object(
    void* target,
    PdfObject* object,
    PdfFieldDescriptor* fields,
    size_t num_fields,
    PdfDocument* doc
);

#define PDF_FIELD(struct_type, key_str, field_name, field_info)                \
    {                                                                          \
        .key = (key_str), .offset = offsetof(struct_type, field_name),         \
        .info = (field_info), .debug = {                                       \
            .file = RELATIVE_FILE_PATH,                                        \
            .line = __LINE__                                                   \
        }                                                                      \
    }

#define PDF_OBJECT_FIELD(object_type_enum)                                     \
    (PdfFieldInfo) {                                                           \
        .kind = PDF_FIELD_KIND_OBJECT, .data.object.type = (object_type_enum)  \
    }

#define PDF_REF_FIELD(ref_type)                                                \
    (PdfFieldInfo) {                                                           \
        .kind = PDF_FIELD_KIND_REF,                                            \
        .data.ref.object_ref_offset = offsetof(ref_type, ref)                  \
    }

#define PDF_ARRAY_FIELD(array_type, element_type, field_info)                  \
    (PdfFieldInfo) {                                                           \
        .kind = PDF_FIELD_KIND_ARRAY, .data.array = {                          \
            .vec_offset = offsetof(array_type, elements),                      \
            .element_size = sizeof(element_type),                              \
            .element_info = (PdfFieldInfo*)&(field_info)                       \
        }                                                                      \
    }

#define PDF_AS_ARRAY_FIELD(array_type, element_type, field_info)               \
    (PdfFieldInfo) {                                                           \
        .kind = PDF_FIELD_KIND_AS_ARRAY, .data.array = {                       \
            .vec_offset = offsetof(array_type, elements),                      \
            .element_size = sizeof(element_type),                              \
            .element_info = (PdfFieldInfo*)&(field_info)                       \
        }                                                                      \
    }

#define PDF_OPTIONAL_FIELD(optional_type, field_info)                          \
    (PdfFieldInfo) {                                                           \
        .kind = PDF_FIELD_KIND_OPTIONAL, .data.optional = {                    \
            .discriminant_offset = offsetof(optional_type, discriminant),      \
            .data_offset = offsetof(optional_type, value),                     \
            .inner_info = (PdfFieldInfo*)&(field_info)                         \
        }                                                                      \
    }

#define PDF_DESERIALIZABLE_REF_IMPL(                                           \
    base_struct,                                                               \
    lowercase_name,                                                            \
    deserialize_fn                                                             \
)                                                                              \
    PdfResult pdf_resolve_##lowercase_name(                                    \
        base_struct##Ref* ref,                                                 \
        PdfDocument* doc,                                                      \
        base_struct* resolved                                                  \
    ) {                                                                        \
        RELEASE_ASSERT(ref);                                                   \
        RELEASE_ASSERT(doc);                                                   \
        RELEASE_ASSERT(resolved);                                              \
        if (ref->resolved) {                                                   \
            *resolved = *(base_struct*)ref->resolved;                          \
            return PDF_OK;                                                     \
        }                                                                      \
        PdfObject* object =                                                    \
            arena_alloc(pdf_doc_arena(doc), sizeof(PdfObject));                \
        PDF_PROPAGATE(pdf_resolve(doc, ref->ref, object));                     \
        PDF_PROPAGATE(deserialize_fn(object, doc, resolved));                  \
        ref->resolved = arena_alloc(pdf_doc_arena(doc), sizeof(base_struct));  \
        *(base_struct*)ref->resolved = *resolved;                              \
        return PDF_OK;                                                         \
    }
