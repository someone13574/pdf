#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "arena/arena.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"

typedef struct PdfDeserdeInfo PdfDeserdeInfo;

/// Deserialization info for optional types.
typedef struct {
    /// Function which initializes an optional and returns a pointer to it's
    /// value field.
    void* (*init_optional)(void* ptr_to_optional, bool has_value);

    /// Deserialization info for the inner type.
    const PdfDeserdeInfo* inner_deserde_info;
} PdfDeserdeOptionalInfo;

/// Function which initializes a resolvable reference with an indirect
/// reference.
typedef void (*PdfDeserdeInitResolvable)(
    void* ptr_to_resolvable,
    PdfIndirectRef ref
);

/// Deserialization info for array types.
typedef struct {
    /// Function which creates a new uninitialized object at the end of a
    /// vector, and returns a pointer to it. If the vector is unallocated, it
    /// will be allocated on `arena`.
    ///
    /// This is used to get around the unknown size and alignment of the element
    /// type by letting the vector (which knows that information) initialize it
    /// for us.
    void* (*vec_push_uninit)(void* ptr_to_vec_ptr, Arena* arena);

    /// Deserialization info for the element type.
    const PdfDeserdeInfo* element_deserde_info;
} PdfDeserdeArrayInfo;

/// Function which deserializes a type from an object.
typedef PdfError* (*PdfDeserdeFn)(
    const PdfObject* object,
    void* target_ptr,
    PdfOptionalResolver resolver,
    Arena* arena
);

/// Gives the required information to deserialize an object.
struct PdfDeserdeInfo {
    enum {
        /// Indicates that deserialization for this type hasn't been implemented
        /// yet.
        PDF_DESERDE_TYPE_UNIMPLEMENTED,

        /// Indicates that this struct contains information for deserializing a
        /// primitive type.
        PDF_DESERDE_TYPE_OBJECT,

        /// Indicates that this struct contains information for deserializing
        /// optional types.
        PDF_DESERDE_TYPE_OPTIONAL,

        /// Indicates that this struct contains information for deserializing an
        /// indirect reference which is lazy-resolved.
        PDF_DESERDE_TYPE_RESOLVABLE,

        /// Indicates that this struct contains information for deserializing to
        /// an array.
        PDF_DESERDE_TYPE_ARRAY,

        /// Indicates that this struct contains information for deserializing to
        /// an array, and singular objects will be treated as a sole array
        /// element.
        PDF_DESERDE_TYPE_AS_ARRAY,

        /// Indicates that this struct contains information for deserializing a
        /// custom type.
        PDF_DESERDE_TYPE_CUSTOM
    } type;

    union {
        PdfObjectType object_info;
        PdfDeserdeOptionalInfo optional_info;
        PdfDeserdeInitResolvable resolvable_info;
        PdfDeserdeArrayInfo array_info; // Used by ARRAY and AS_ARRAY
        PdfDeserdeFn custom_fn;
    } value;
};

/// Provides information required to deserialize a dictionary field into a
/// struct member.
typedef struct {
    /// The key as it appears as a name in the dictionary.
    const char* key;

    /// A pointer to the struct member to which this field should be
    /// deserialized into.
    void* target_ptr;

    /// Information required to deserialize this field.
    PdfDeserdeInfo deserde_info;

    /// Additional debug info for logging and errors indicating the source of
    /// the field information.
    struct {
        const char* file;
        unsigned long line;
    } debug_info;
} PdfFieldDescriptor;

/// Deserializes a PDF dictionary, possibly behind an indirect reference or in
/// an indirect object, into a struct.
PdfError* pdf_deserialize_dict(
    const PdfObject* object,
    const PdfFieldDescriptor* fields,
    size_t num_fields,
    bool allow_unknown_fields,
    PdfOptionalResolver resolver,
    Arena* arena, // An arena is required so arrays can be allocated.
    const char* debug_name
);

typedef struct {
    /// A pointer to the variable to which this operand should be deserialized
    /// into.
    void* target_ptr;

    /// Information required to deserialize this operand.
    PdfDeserdeInfo deserde_info;

    /// Additional debug info for logging and errors indicating the source of
    /// the deserialization information.
    struct {
        const char* file;
        unsigned long line;
    } debug_info;
} PdfOperandDescriptor;

/// Deserializes an array of objects. Note that this will not attempt to resolve
/// references.
PdfError* pdf_deserialize_operands(
    const PdfObjectVec* operands,
    void* target_ptr,
    const PdfOperandDescriptor* descriptors,
    size_t num_descriptors,
    Arena* arena
);

#define PDF_OPERAND(ptr_to_variable, operand_information)                      \
    {                                                                          \
        .target_ptr = (ptr_to_variable),                                       \
        .deserde_info = (operand_information),                                 \
        .debug_info.file = RELATIVE_FILE_PATH, .debug_info.line = __LINE__     \
    }

/// Declares a new dictionary field which should be deserialized.
///
/// - `pdf_key_name`: A literal string representing the name key in the
/// dictionary.
/// - `ptr_to_field`: A pointer to the struct member which this field should be
/// deserialized into.
/// - `field_information`: The deserialization information for this field.
#define PDF_FIELD(pdf_key_name, ptr_to_field, field_information)               \
    {                                                                          \
        .key = (pdf_key_name), .target_ptr = (ptr_to_field),                   \
        .deserde_info = (field_information),                                   \
        .debug_info.file = RELATIVE_FILE_PATH, .debug_info.line = __LINE__     \
    }

/// Marks that deserialization hasn't been implemented for this yet.
#define PDF_UNIMPLEMENTED_FIELD(pdf_key_name)                                  \
    PDF_FIELD(                                                                 \
        pdf_key_name,                                                          \
        NULL,                                                                  \
        (PdfDeserdeInfo) {.type = PDF_DESERDE_TYPE_UNIMPLEMENTED}              \
    )

#define PDF_DESERDE_OBJECT(object_type)                                        \
    (PdfDeserdeInfo) {                                                         \
        .type = PDF_DESERDE_TYPE_OBJECT, .value.object_info = (object_type)    \
    }

#define PDF_DESERDE_OPTIONAL(optional_init_fn, inner_info)                     \
    (PdfDeserdeInfo) {                                                         \
        .type = PDF_DESERDE_TYPE_OPTIONAL,                                     \
        .value.optional_info.init_optional = (optional_init_fn),               \
        .value.optional_info.inner_deserde_info =                              \
            (PdfDeserdeInfo*)&(inner_info)                                     \
    }

#define PDF_DESERDE_RESOLVABLE(resolvable_init_fn)                             \
    (PdfDeserdeInfo) {                                                         \
        .type = PDF_DESERDE_TYPE_RESOLVABLE,                                   \
        .value.resolvable_info = (resolvable_init_fn)                          \
    }

#define PDF_DESERDE_ARRAY(vec_push_uninit_fn, element_info)                    \
    (PdfDeserdeInfo) {                                                         \
        .type = PDF_DESERDE_TYPE_ARRAY,                                        \
        .value.array_info.vec_push_uninit = (vec_push_uninit_fn),              \
        .value.array_info.element_deserde_info =                               \
            (PdfDeserdeInfo*)&(element_info)                                   \
    }

#define PDF_DESERDE_AS_ARRAY(vec_push_uninit_fn, element_info)                 \
    (PdfDeserdeInfo) {                                                         \
        .type = PDF_DESERDE_TYPE_AS_ARRAY,                                     \
        .value.array_info.vec_push_uninit = (vec_push_uninit_fn),              \
        .value.array_info.element_deserde_info =                               \
            (PdfDeserdeInfo*)&(element_info)                                   \
    }

#define PDF_DESERDE_CUSTOM(deserialization_fn)                                 \
    (PdfDeserdeInfo) {                                                         \
        .type = PDF_DESERDE_TYPE_CUSTOM,                                       \
        .value.custom_fn = (deserialization_fn)                                \
    }

#define DESERDE_IMPL_OPTIONAL(new_type, init_fn)                               \
    void* init_fn(void* ptr_to_optional, _Bool has_value) {                    \
        new_type* optional = ptr_to_optional;                                  \
        optional->has_value = has_value;                                       \
        return &optional->value;                                               \
    }

#define DESERDE_IMPL_RESOLVABLE(                                               \
    new_type,                                                                  \
    base_type,                                                                 \
    init_fn,                                                                   \
    resolve_fn,                                                                \
    deserde_fn                                                                 \
)                                                                              \
    void init_fn(void* ptr_to_resolvable, PdfIndirectRef ref) {                \
        new_type* resolvable = ptr_to_resolvable;                              \
        resolvable->ref = ref;                                                 \
        resolvable->resolved = NULL;                                           \
    }                                                                          \
    PdfError* resolve_fn(                                                      \
        new_type resolvable,                                                   \
        PdfResolver* resolver,                                                 \
        base_type* resolved_out                                                \
    ) {                                                                        \
        RELEASE_ASSERT(resolver);                                              \
        RELEASE_ASSERT(resolved_out);                                          \
        if (resolvable.resolved) {                                             \
            *resolved_out = *resolvable.resolved;                              \
            return NULL;                                                       \
        }                                                                      \
        PdfObject resolved;                                                    \
        PDF_PROPAGATE(pdf_resolve_ref(resolver, resolvable.ref, &resolved));   \
        resolvable.resolved =                                                  \
            arena_alloc(pdf_resolver_arena(resolver), sizeof(base_type));      \
        PDF_PROPAGATE(deserde_fn(                                              \
            &resolved,                                                         \
            resolvable.resolved,                                               \
            pdf_op_resolver_some(resolver),                                    \
            pdf_resolver_arena(resolver)                                       \
        ));                                                                    \
        *resolved_out = *resolvable.resolved;                                  \
        return NULL;                                                           \
    }

#define DESERDE_IMPL_TRAMPOLINE(trampoline_fn, deserde_fn)                     \
    PdfError* trampoline_fn(                                                   \
        const PdfObject* object,                                               \
        void* target_ptr,                                                      \
        PdfOptionalResolver resolver,                                          \
        Arena* arena                                                           \
    ) {                                                                        \
        return deserde_fn(object, target_ptr, resolver, arena);                \
    }
