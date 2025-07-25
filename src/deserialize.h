#pragma once

#include <stddef.h>

#include "log.h"
#include "pdf/error.h"
#include "pdf/object.h"
#include "pdf/resolver.h"

typedef enum {
    PDF_FIELD_KIND_OBJECT,
    PDF_FIELD_KIND_REF,
    PDF_FIELD_KIND_CUSTOM,
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

typedef PdfError* (*PdfDeserializerFn)(
    PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    void* deserialized
);

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
        PdfDeserializerFn custom;
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

typedef struct {
    size_t offset;
    PdfFieldInfo info;
    PdfFieldDebugInfo debug;
} PdfOperandDescriptor;

PdfError* pdf_resolve_object(
    PdfOptionalResolver resolver,
    PdfObject* object,
    PdfObject* resolved
);

PdfError* pdf_deserialize_object_field(
    void* field_ptr,
    PdfObject* object,
    PdfObjectFieldData field_data
);
PdfError* pdf_deserialize_ref_field(
    void* field_ptr,
    PdfObject* object,
    PdfRefFieldData field_data
);
PdfError* pdf_deserialize_custom_field(
    void* field_ptr,
    PdfObject* object,
    Arena* arena,
    PdfDeserializerFn deserializer,
    PdfOptionalResolver resolver
);
PdfError* pdf_deserialize_array_field(
    void* field_ptr,
    PdfObject* object,
    PdfArrayFieldData field_data,
    Arena* arena,
    PdfOptionalResolver resolver
);
PdfError* pdf_deserialize_as_array_field(
    void* field_ptr,
    PdfObject* object,
    PdfArrayFieldData field_data,
    Arena* arena,
    PdfOptionalResolver resolver
);
PdfError* pdf_deserialize_optional_field(
    void* field_ptr,
    PdfObject* object,
    PdfOptionalFieldData field_data,
    Arena* arena,
    PdfOptionalResolver resolver
);

PdfError* pdf_deserialize_object(
    void* target,
    PdfObject* object,
    PdfFieldDescriptor* fields,
    size_t num_fields,
    Arena* arena,
    PdfOptionalResolver resolver
);

PdfError* pdf_deserialize_operands(
    void* target,
    PdfOperandDescriptor* descriptors,
    size_t num_descriptors,
    PdfObjectVec* operands,
    Arena* arena
);

#define PDF_FIELD(struct_type, key_str, field_name, field_info)                \
    {                                                                          \
        .key = (key_str), .offset = offsetof(struct_type, field_name),         \
        .info = (field_info), .debug = {                                       \
            .file = RELATIVE_FILE_PATH,                                        \
            .line = __LINE__                                                   \
        }                                                                      \
    }

#define PDF_OPERAND(struct_type, field_name, field_info)                       \
    {                                                                          \
        .offset = offsetof(struct_type, field_name), .info = (field_info),     \
        .debug = {                                                             \
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

#define PDF_CUSTOM_FIELD(deserializer_fn)                                      \
    (PdfFieldInfo) {                                                           \
        .kind = PDF_FIELD_KIND_CUSTOM, .data.custom = deserializer_fn          \
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
    PdfError* pdf_resolve_##lowercase_name(                                    \
        base_struct##Ref* ref,                                                 \
        PdfResolver* resolver,                                                 \
        base_struct* resolved                                                  \
    ) {                                                                        \
        RELEASE_ASSERT(ref);                                                   \
        RELEASE_ASSERT(resolver);                                              \
        RELEASE_ASSERT(resolved);                                              \
        if (ref->resolved) {                                                   \
            *resolved = *(base_struct*)ref->resolved;                          \
            return NULL;                                                       \
        }                                                                      \
        PdfObject* object =                                                    \
            arena_alloc(pdf_resolver_arena(resolver), sizeof(PdfObject));      \
        PDF_PROPAGATE(pdf_resolve_ref(resolver, ref->ref, object));            \
        PDF_PROPAGATE(deserialize_fn(                                          \
            object,                                                            \
            pdf_resolver_arena(resolver),                                      \
            pdf_op_resolver_some(resolver),                                    \
            resolved                                                           \
        ));                                                                    \
        ref->resolved =                                                        \
            arena_alloc(pdf_resolver_arena(resolver), sizeof(base_struct));    \
        *(base_struct*)ref->resolved = *resolved;                              \
        return NULL;                                                           \
    }

#define PDF_UNTYPED_DESERIALIZER_WRAPPER(deserialize_fn, wrapper_name)         \
    PdfError* wrapper_name(                                                    \
        PdfObject* object,                                                     \
        Arena* arena,                                                          \
        PdfOptionalResolver resolver,                                          \
        void* deserialized                                                     \
    ) {                                                                        \
        return deserialize_fn(object, arena, resolver, deserialized);          \
    }
