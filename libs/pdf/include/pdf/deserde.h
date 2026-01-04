#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "arena/arena.h"
#include "err/error.h"
#include "logger/log.h"
#include "pdf/object.h"
#include "pdf/resolver.h"

typedef Error* (*PdfDeserdeFn)(
    const PdfObject* object,
    void* target_ptr,
    PdfResolver* resolver
);

typedef struct PdfFieldDescriptor PdfFieldDescriptor;
typedef Error* (*PdfOnFieldMissing)(PdfFieldDescriptor descriptor);
typedef Error* (*PdfDeserdeFixedArrayFn)(
    const PdfObject* object,
    PdfFieldDescriptor descriptor,
    PdfResolver* resolver
);

struct PdfFieldDescriptor {
    const char* key;
    void* target_ptr;
    PdfDeserdeFn deserializer;
    PdfOnFieldMissing on_missing;
    int32_t fixed_array_len; /// If this field is positive, then the
                             /// deserializer will attempt to deserialize an
                             /// array made up of elements of which
                             /// `deserializer` and deserialize.
    const void* fixed_array_default;
    PdfDeserdeFixedArrayFn fixed_array_deserializer;
};

typedef struct {
    void* target_ptr;
    PdfDeserdeFn deserializer;
} PdfOperandDescriptor;

Error* pdf_deserde_fields(
    const PdfObject* object,
    const PdfFieldDescriptor* fields,
    size_t num_fields,
    bool allow_unknown_fields,
    PdfResolver* resolver,
    const char* debug_name
);

Error* pdf_deserde_operands(
    const PdfObjectVec* operands,
    const PdfOperandDescriptor* descriptors,
    size_t num_descriptors,
    PdfResolver* resolver
);

/// The caller must create the array before this function, otherwise empty
/// arrays may be uninitialized
Error* pdf_deserde_typed_array(
    const PdfObject* object,
    PdfDeserdeFn deserializer,
    PdfResolver* resolver,
    bool allow_single_element,
    void* (*push_uninit)(void* ptr_to_vec_ptr, Arena* arena),
    void** ptr_to_vec_ptr
);

Error* pdf_deserde_boolean(
    const PdfObject* object,
    PdfBoolean* target_ptr,
    PdfResolver* resolver
);

Error* pdf_deserde_integer(
    const PdfObject* object,
    PdfInteger* target_ptr,
    PdfResolver* resolver
);

Error* pdf_deserde_real(
    const PdfObject* object,
    PdfReal* target_ptr,
    PdfResolver* resolver
);

Error* pdf_deserde_string(
    const PdfObject* object,
    PdfString* target_ptr,
    PdfResolver* resolver
);

Error* pdf_deserde_name(
    const PdfObject* object,
    PdfName* target_ptr,
    PdfResolver* resolver
);

Error* pdf_deserde_array(
    const PdfObject* object,
    PdfArray* target_ptr,
    PdfResolver* resolver
);

Error* pdf_deserde_dict(
    const PdfObject* object,
    PdfDict* target_ptr,
    PdfResolver* resolver
);

Error* pdf_deserde_stream(
    const PdfObject* object,
    PdfStream* target_ptr,
    PdfResolver* resolver
);

Error* pdf_deserde_indirect_object(
    const PdfObject* object,
    PdfIndirectObject* target_ptr,
    PdfResolver* resolver
);

Error* pdf_deserde_indirect_ref(
    const PdfObject* object,
    PdfIndirectRef* target_ptr,
    PdfResolver* resolver
);

typedef int PdfUnimplemented;
typedef struct {
    bool is_some;
    PdfObject object;
} PdfIgnored;

PdfFieldDescriptor pdf_unimplemented_field(const char* key);
PdfFieldDescriptor pdf_ignored_field(const char* key, PdfIgnored* target_ptr);

#define PDF_DECL_FIELD(field_type, lowercase)                                  \
    Error* pdf_deserde_##lowercase##_trampoline(                               \
        const PdfObject* object,                                               \
        void* target_ptr,                                                      \
        PdfResolver* resolver                                                  \
    );                                                                         \
    PdfFieldDescriptor pdf_##lowercase##_field(                                \
        const char* key,                                                       \
        field_type* target_ptr                                                 \
    );                                                                         \
    PdfOperandDescriptor pdf_##lowercase##_operand(field_type* target_ptr);

#define PDF_IMPL_FIELD(field_type, lowercase)                                  \
    Error* pdf_deserde_##lowercase##_trampoline(                               \
        const PdfObject* object,                                               \
        void* target_ptr,                                                      \
        PdfResolver* resolver                                                  \
    ) {                                                                        \
        return pdf_deserde_##lowercase(                                        \
            object,                                                            \
            (field_type*)target_ptr,                                           \
            resolver                                                           \
        );                                                                     \
    }                                                                          \
    PdfFieldDescriptor pdf_##lowercase##_field(                                \
        const char* key,                                                       \
        field_type* target_ptr                                                 \
    ) {                                                                        \
        RELEASE_ASSERT(key);                                                   \
        RELEASE_ASSERT(target_ptr);                                            \
        return (PdfFieldDescriptor) {.key = key,                               \
                                     .target_ptr = (void*)target_ptr,          \
                                     .deserializer =                           \
                                         pdf_deserde_##lowercase##_trampoline, \
                                     .on_missing = NULL,                       \
                                     .fixed_array_len = -1,                    \
                                     .fixed_array_default = NULL,              \
                                     .fixed_array_deserializer = NULL};        \
    }                                                                          \
    PdfOperandDescriptor pdf_##lowercase##_operand(field_type* target_ptr) {   \
        return (PdfOperandDescriptor) {                                        \
            .target_ptr = (void*)target_ptr,                                   \
            .deserializer = pdf_deserde_##lowercase##_trampoline               \
        };                                                                     \
    }

#define PDF_DECL_OPTIONAL_FIELD(base_type, optional_type, lowercase_base)      \
    typedef struct {                                                           \
        bool is_some;                                                          \
        base_type value;                                                       \
    } optional_type;                                                           \
    PdfFieldDescriptor pdf_##lowercase_base##_optional_field(                  \
        const char* key,                                                       \
        optional_type* target_ptr                                              \
    );

#define PDF_IMPL_OPTIONAL_FIELD(base_type, optional_type, lowercase_base)      \
    Error* pdf_deserde_##lowercase_base##_optional_trampoline(                 \
        const PdfObject* object,                                               \
        void* target_ptr,                                                      \
        PdfResolver* resolver                                                  \
    ) {                                                                        \
        RELEASE_ASSERT(target_ptr);                                            \
        optional_type* target = target_ptr;                                    \
        target->is_some = true;                                                \
        return pdf_deserde_##lowercase_base(                                   \
            object,                                                            \
            (base_type*)&target->value,                                        \
            resolver                                                           \
        );                                                                     \
    }                                                                          \
    Error* pdf_##lowercase_base##_init_none(PdfFieldDescriptor descriptor) {   \
        RELEASE_ASSERT(descriptor.target_ptr);                                 \
        optional_type* target = descriptor.target_ptr;                         \
        target->is_some = false;                                               \
        return NULL;                                                           \
    }                                                                          \
    PdfFieldDescriptor pdf_##lowercase_base##_optional_field(                  \
        const char* key,                                                       \
        optional_type* target_ptr                                              \
    ) {                                                                        \
        RELEASE_ASSERT(key);                                                   \
        RELEASE_ASSERT(target_ptr);                                            \
        return (                                                               \
            PdfFieldDescriptor                                                 \
        ) {.key = key,                                                         \
           .target_ptr = (void*)target_ptr,                                    \
           .deserializer = pdf_deserde_##lowercase_base##_optional_trampoline, \
           .on_missing = pdf_##lowercase_base##_init_none,                     \
           .fixed_array_len = -1,                                              \
           .fixed_array_default = NULL,                                        \
           .fixed_array_deserializer = NULL};                                  \
    }

#define PDF_DECL_ARRAY_FIELD(vec_type, lowercase_vec_type)                     \
    PDF_DECL_FIELD(vec_type*, lowercase_vec_type)

#define PDF_IMPL_ARRAY_FIELD(                                                  \
    vec_type,                                                                  \
    lowercase_vec_type,                                                        \
    lowercase_base_type                                                        \
)                                                                              \
    Error* pdf_deserde_##lowercase_vec_type(                                   \
        const PdfObject* object,                                               \
        vec_type** target_ptr,                                                 \
        PdfResolver* resolver                                                  \
    ) {                                                                        \
        RELEASE_ASSERT(target_ptr);                                            \
        RELEASE_ASSERT(resolver);                                              \
        *target_ptr =                                                          \
            pdf_##lowercase_vec_type##_new(pdf_resolver_arena(resolver));      \
        TRY(pdf_deserde_typed_array(                                           \
            object,                                                            \
            pdf_deserde_##lowercase_base_type##_trampoline,                    \
            resolver,                                                          \
            false,                                                             \
            pdf_##lowercase_vec_type##_push_uninit,                            \
            (void**)target_ptr                                                 \
        ));                                                                    \
        return NULL;                                                           \
    }                                                                          \
    PDF_IMPL_FIELD(vec_type*, lowercase_vec_type)

#define PDF_DECL_AS_ARRAY_FIELD(vec_type, lowercase_vec_type)                  \
    PDF_DECL_FIELD(vec_type*, as_##lowercase_vec_type)

#define PDF_IMPL_AS_ARRAY_FIELD(                                               \
    vec_type,                                                                  \
    lowercase_vec_type,                                                        \
    lowercase_base_type                                                        \
)                                                                              \
    Error* pdf_deserde_as_##lowercase_vec_type(                                \
        const PdfObject* object,                                               \
        vec_type** target_ptr,                                                 \
        PdfResolver* resolver                                                  \
    ) {                                                                        \
        RELEASE_ASSERT(target_ptr);                                            \
        RELEASE_ASSERT(resolver);                                              \
        *target_ptr =                                                          \
            pdf_##lowercase_vec_type##_new(pdf_resolver_arena(resolver));      \
        TRY(pdf_deserde_typed_array(                                           \
            object,                                                            \
            pdf_deserde_##lowercase_base_type##_trampoline,                    \
            resolver,                                                          \
            true,                                                              \
            pdf_##lowercase_vec_type##_push_uninit,                            \
            (void**)target_ptr                                                 \
        ));                                                                    \
        return NULL;                                                           \
    }                                                                          \
    PDF_IMPL_FIELD(vec_type*, as_##lowercase_vec_type)

#define PDF_DECL_FIXED_ARRAY_FIELD(element_type, lowercase_base)               \
    PdfFieldDescriptor pdf_##lowercase_base##_fixed_array_field(               \
        const char* key,                                                       \
        element_type* target_ptr,                                              \
        uint16_t length,                                                       \
        const element_type* default_array                                      \
    );

#define PDF_IMPL_FIXED_ARRAY_FIELD(element_type, lowercase_base)               \
    Error* pdf_deserde_##lowercase_base##_fix_array(                           \
        const PdfObject* object,                                               \
        PdfFieldDescriptor descriptor,                                         \
        PdfResolver* resolver                                                  \
    ) {                                                                        \
        RELEASE_ASSERT(object);                                                \
        RELEASE_ASSERT(descriptor.target_ptr);                                 \
        RELEASE_ASSERT(resolver);                                              \
        PdfArray array;                                                        \
        TRY(pdf_deserde_array(object, &array, resolver));                      \
        size_t actual_len = pdf_object_vec_len(array.elements);                \
        if ((size_t)descriptor.fixed_array_len != actual_len) {                \
            return ERROR(                                                      \
                PDF_ERR_INCORRECT_TYPE,                                        \
                "Incorrect array length. Expected %zu, found %zu",             \
                (size_t)descriptor.fixed_array_len,                            \
                actual_len                                                     \
            );                                                                 \
        }                                                                      \
        element_type* target_array = (element_type*)descriptor.target_ptr;     \
        for (size_t idx = 0; idx < actual_len; idx++) {                        \
            PdfObject element;                                                 \
            RELEASE_ASSERT(pdf_object_vec_get(array.elements, idx, &element)); \
            element_type deserialized;                                         \
            TRY(descriptor.deserializer(&element, &deserialized, resolver));   \
            target_array[idx] = deserialized;                                  \
        }                                                                      \
        return NULL;                                                           \
    }                                                                          \
    Error* pdf_##lowercase_base##_fixed_array_set_default(                     \
        PdfFieldDescriptor descriptor                                          \
    ) {                                                                        \
        RELEASE_ASSERT(descriptor.target_ptr);                                 \
        RELEASE_ASSERT(descriptor.fixed_array_len >= 0);                       \
        RELEASE_ASSERT(descriptor.fixed_array_default);                        \
        const element_type* src = descriptor.fixed_array_default;              \
        element_type* dst = descriptor.target_ptr;                             \
        for (int32_t idx = 0; idx < descriptor.fixed_array_len; idx++) {       \
            dst[idx] = src[idx];                                               \
        }                                                                      \
        return NULL;                                                           \
    }                                                                          \
    PdfFieldDescriptor pdf_##lowercase_base##_fixed_array_field(               \
        const char* key,                                                       \
        element_type* target_ptr,                                              \
        uint16_t length,                                                       \
        const element_type* default_array                                      \
    ) {                                                                        \
        RELEASE_ASSERT(key);                                                   \
        RELEASE_ASSERT(target_ptr);                                            \
        return (PdfFieldDescriptor) {                                          \
            .key = key,                                                        \
            .target_ptr = (void*)target_ptr,                                   \
            .deserializer = pdf_deserde_##lowercase_base##_trampoline,         \
            .on_missing = default_array                                        \
                            ? pdf_##lowercase_base##_fixed_array_set_default   \
                            : NULL,                                            \
            .fixed_array_len = (int32_t)length,                                \
            .fixed_array_default = (void*)default_array,                       \
            .fixed_array_deserializer =                                        \
                pdf_deserde_##lowercase_base##_fix_array                       \
        };                                                                     \
    }

#define PDF_DECL_RESOLVABLE_FIELD(base_type, ref_type, lowercase_base)         \
    typedef struct {                                                           \
        PdfIndirectRef ref;                                                    \
        base_type* resolved;                                                   \
    } ref_type;                                                                \
    Error* pdf_resolve_##lowercase_base(ref_type* ref, PdfResolver* resolver); \
    PDF_DECL_FIELD(ref_type, lowercase_base##_ref)

#define PDF_IMPL_RESOLVABLE_FIELD(base_type, ref_type, lowercase_base)         \
    Error* pdf_resolve_##lowercase_base(                                       \
        ref_type* ref,                                                         \
        PdfResolver* resolver                                                  \
    ) {                                                                        \
        RELEASE_ASSERT(ref);                                                   \
        RELEASE_ASSERT(resolver);                                              \
        if (ref->resolved) {                                                   \
            return NULL;                                                       \
        }                                                                      \
        PdfObject resolved;                                                    \
        TRY(pdf_resolve_ref(resolver, ref->ref, &resolved));                   \
        ref->resolved =                                                        \
            arena_alloc(pdf_resolver_arena(resolver), sizeof(base_type));      \
        TRY(pdf_deserde_##lowercase_base(&resolved, ref->resolved, resolver)); \
        return NULL;                                                           \
    }                                                                          \
    Error* pdf_deserde_##lowercase_base##_ref(                                 \
        const PdfObject* object,                                               \
        ref_type* target_ptr,                                                  \
        PdfResolver* resolver                                                  \
    ) {                                                                        \
        RELEASE_ASSERT(object);                                                \
        RELEASE_ASSERT(target_ptr);                                            \
        RELEASE_ASSERT(resolver);                                              \
        target_ptr->resolved = NULL;                                           \
        TRY(pdf_deserde_indirect_ref(object, &target_ptr->ref, resolver));     \
        return NULL;                                                           \
    }                                                                          \
    PDF_IMPL_FIELD(ref_type, lowercase_base##_ref)
