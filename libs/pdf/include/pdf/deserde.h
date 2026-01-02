#pragma once

#include <stdbool.h>

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

typedef void (*PdfInitNoneFn)(void* target_ptr);

typedef struct {
    const char* key;
    void* target_ptr;
    PdfDeserdeFn deserializer;
    PdfInitNoneFn init_none;
} PdfFieldDescriptor;

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

Error* pdf_deserde_typed_array(
    const PdfObject* object,
    PdfDeserdeFn deserializer,
    PdfResolver* resolver,
    bool allow_single_element,
    void* (*push_uninit)(void* ptr_to_vec_ptr, Arena* arena),
    void** ptr_to_vec_ptr
);

Error* pdf_deserde_operands(
    const PdfObjectVec* operands,
    const PdfOperandDescriptor* descriptors,
    size_t num_descriptors,
    PdfResolver* resolver
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

PdfFieldDescriptor pdf_unimplemented_field(const char* key);
PdfFieldDescriptor pdf_ignored_field(const char* key, PdfObject* target_ptr);

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
        return (PdfFieldDescriptor) {.key = key,                               \
                                     .target_ptr = (void*)target_ptr,          \
                                     .deserializer =                           \
                                         pdf_deserde_##lowercase##_trampoline, \
                                     .init_none = NULL};                       \
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
        optional_type* target = target_ptr;                                    \
        target->is_some = true;                                                \
        return pdf_deserde_##lowercase_base(                                   \
            object,                                                            \
            (base_type*)&target->value,                                        \
            resolver                                                           \
        );                                                                     \
    }                                                                          \
    void pdf_##lowercase_base##_init_none(void* target_ptr) {                  \
        optional_type* target = target_ptr;                                    \
        target->is_some = false;                                               \
    }                                                                          \
    PdfFieldDescriptor pdf_##lowercase_base##_optional_field(                  \
        const char* key,                                                       \
        optional_type* target_ptr                                              \
    ) {                                                                        \
        return (                                                               \
            PdfFieldDescriptor                                                 \
        ) {.key = key,                                                         \
           .target_ptr = (void*)target_ptr,                                    \
           .deserializer = pdf_deserde_##lowercase_base##_optional_trampoline, \
           .init_none = pdf_##lowercase_base##_init_none};                     \
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
            arena_alloc(pdf_resolver_arena(resolver), sizeof(ref_type));       \
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
