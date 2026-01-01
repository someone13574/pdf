#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "arena/arena.h"
#include "err/error.h"
#include "pdf/object.h"
#include "pdf/resolver.h"

typedef struct PdfDeserInfo PdfDeserInfo;

/// Deserialization info for optional types.
typedef struct {
    /// Function which initializes an optional and returns a pointer to it's
    /// value field.
    void* (*init_optional)(void* ptr_to_optional, bool has_value);

    /// Deserialization info for the inner type.
    const PdfDeserInfo* inner_deser_info;
} PdfDeserOptionalInfo;

/// Function which initializes a resolvable reference with an indirect
/// reference.
typedef void (*PdfDeserInitResolvable)(
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
    const PdfDeserInfo* element_deser_info;
} PdfDeserArrayInfo;

/// Function which desers a type from an object.
typedef Error* (*PdfDeserFn)(
    const PdfObject* object,
    void* target_ptr,
    PdfResolver* resolver
);

/// Gives the required information to deser an object.
struct PdfDeserInfo {
    enum {
        /// Indicates that deserialization for this type hasn't been implemented
        /// yet.
        PDF_DESER_TYPE_UNIMPLEMENTED,

        /// Indicates that this field is known and optional, but should be
        /// skipped.
        PDF_DESER_TYPE_IGNORED,

        /// Indicates that this struct contains information for deserializing a
        /// primitive type.
        PDF_DESER_TYPE_OBJECT,

        /// Indicates that this struct contains information for deserializing
        /// optional types.
        PDF_DESER_TYPE_OPTIONAL,

        /// Indicates that this struct contains information for deserializing an
        /// indirect reference which is lazy-resolved.
        PDF_DESER_TYPE_RESOLVABLE,

        /// Indicates that this struct contains information for deserializing to
        /// an array.
        PDF_DESER_TYPE_ARRAY,

        /// Indicates that this struct contains information for deserializing to
        /// an array, and singular objects will be treated as a sole array
        /// element.
        PDF_DESER_TYPE_AS_ARRAY,

        /// Indicates that this struct contains information for deserializing a
        /// custom type.
        PDF_DESER_TYPE_CUSTOM
    } type;

    union {
        PdfObjectType object_info;
        PdfDeserOptionalInfo optional_info;
        PdfDeserInitResolvable resolvable_info;
        PdfDeserArrayInfo array_info; // Used by ARRAY and AS_ARRAY
        PdfDeserFn custom_fn;
    } value;
};

/// Provides information required to deser a dictionary field into a
/// struct member.
typedef struct {
    /// The key as it appears as a name in the dictionary.
    const char* key;

    /// A pointer to the struct member to which this field should be
    /// deserialized into.
    void* target_ptr;

    /// Information required to deser this field.
    PdfDeserInfo deser_info;

    /// Additional debug info for logging and errors indicating the source of
    /// the field information.
    struct {
        const char* file;
        unsigned long line;
    } debug_info;
} PdfFieldDescriptor;

/// Deserializes a PDF dictionary, possibly behind an indirect reference or in
/// an indirect object, into a struct.
Error* pdf_deser_dict(
    const PdfObject* object,
    const PdfFieldDescriptor* fields,
    size_t num_fields,
    bool allow_unknown_fields,
    PdfResolver* resolver,
    const char* debug_name
);

typedef struct {
    /// A pointer to the variable to which this operand should be deserialized
    /// into.
    void* target_ptr;

    /// Information required to deser this operand.
    PdfDeserInfo deser_info;

    /// Additional debug info for logging and errors indicating the source of
    /// the deserialization information.
    struct {
        const char* file;
        unsigned long line;
    } debug_info;
} PdfOperandDescriptor;

/// Deserializes an array of objects. Note that this will not attempt to resolve
/// references.
Error* pdf_deser_operands(
    const PdfObjectVec* operands,
    const PdfOperandDescriptor* descriptors,
    size_t num_descriptors,
    PdfResolver* resolver
);

#define PDF_OPERAND(ptr_to_variable, operand_information)                      \
    {.target_ptr = (void*)(ptr_to_variable),                                   \
     .deser_info = (operand_information),                                      \
     .debug_info.file = RELATIVE_FILE_PATH,                                    \
     .debug_info.line = __LINE__}

/// Declares a new dictionary field which should be deserialized.
///
/// - `pdf_key_name`: A literal string representing the name key in the
/// dictionary.
/// - `ptr_to_field`: A pointer to the struct member which this field should be
/// deserialized into.
/// - `field_information`: The deserialization information for this field.
#define PDF_FIELD(pdf_key_name, ptr_to_field, field_information)               \
    {.key = (pdf_key_name),                                                    \
     .target_ptr = (void*)(ptr_to_field),                                      \
     .deser_info = (field_information),                                        \
     .debug_info.file = RELATIVE_FILE_PATH,                                    \
     .debug_info.line = __LINE__}

/// Marks that deserialization hasn't been implemented for this yet. Panics if
/// the field is encountered.
#define PDF_UNIMPLEMENTED_FIELD(pdf_key_name)                                  \
    PDF_FIELD(                                                                 \
        pdf_key_name,                                                          \
        NULL,                                                                  \
        (PdfDeserInfo) {.type = PDF_DESER_TYPE_UNIMPLEMENTED}                  \
    )

/// Marks that deserialization hasn't been implemented for this yet. Sets the
/// object if found, sets to null otherwise.
#define PDF_IGNORED_FIELD(pdf_key_name, ptr_to_field)                          \
    {.key = (pdf_key_name),                                                    \
     .target_ptr = (void*)(ptr_to_field),                                      \
     .deser_info = (PdfDeserInfo) {.type = PDF_DESER_TYPE_IGNORED},            \
     .debug_info.file = RELATIVE_FILE_PATH,                                    \
     .debug_info.line = __LINE__}

#define PDF_DESER_OBJECT(object_type)                                          \
    (PdfDeserInfo) {                                                           \
        .type = PDF_DESER_TYPE_OBJECT, .value.object_info = (object_type)      \
    }

#define PDF_DESER_OPTIONAL(optional_init_fn, inner_info)                       \
    (PdfDeserInfo) {                                                           \
        .type = PDF_DESER_TYPE_OPTIONAL,                                       \
        .value.optional_info.init_optional = (optional_init_fn),               \
        .value.optional_info.inner_deser_info = (PdfDeserInfo*)&(inner_info)   \
    }

#define PDF_DESER_RESOLVABLE(resolvable_init_fn)                               \
    (PdfDeserInfo) {                                                           \
        .type = PDF_DESER_TYPE_RESOLVABLE,                                     \
        .value.resolvable_info = (resolvable_init_fn)                          \
    }

#define PDF_DESER_ARRAY(vec_push_uninit_fn, element_info)                      \
    (PdfDeserInfo) {                                                           \
        .type = PDF_DESER_TYPE_ARRAY,                                          \
        .value.array_info.vec_push_uninit = (vec_push_uninit_fn),              \
        .value.array_info.element_deser_info = (PdfDeserInfo*)&(element_info)  \
    }

#define PDF_DESER_AS_ARRAY(vec_push_uninit_fn, element_info)                   \
    (PdfDeserInfo) {                                                           \
        .type = PDF_DESER_TYPE_AS_ARRAY,                                       \
        .value.array_info.vec_push_uninit = (vec_push_uninit_fn),              \
        .value.array_info.element_deser_info = (PdfDeserInfo*)&(element_info)  \
    }

#define PDF_DESER_CUSTOM(deserialization_fn)                                   \
    (PdfDeserInfo) {                                                           \
        .type = PDF_DESER_TYPE_CUSTOM, .value.custom_fn = (deserialization_fn) \
    }

#define DESER_IMPL_OPTIONAL(new_type, init_fn)                                 \
    void* init_fn(void* ptr_to_optional, _Bool has_value) {                    \
        new_type* optional = ptr_to_optional;                                  \
        optional->has_value = has_value;                                       \
        return (void*)&optional->value;                                        \
    }

#define DESER_IMPL_RESOLVABLE(                                                 \
    new_type,                                                                  \
    base_type,                                                                 \
    init_fn,                                                                   \
    resolve_fn,                                                                \
    deser_fn                                                                   \
)                                                                              \
    void init_fn(void* ptr_to_resolvable, PdfIndirectRef ref) {                \
        new_type* resolvable = ptr_to_resolvable;                              \
        resolvable->ref = ref;                                                 \
        resolvable->resolved = NULL;                                           \
    }                                                                          \
    Error* resolve_fn(                                                         \
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
        TRY(pdf_resolve_ref(resolver, resolvable.ref, &resolved));             \
        resolvable.resolved =                                                  \
            arena_alloc(pdf_resolver_arena(resolver), sizeof(base_type));      \
        TRY(deser_fn(&resolved, resolvable.resolved, resolver));               \
        *resolved_out = *resolvable.resolved;                                  \
        return NULL;                                                           \
    }

#define DESER_IMPL_TRAMPOLINE(trampoline_fn, deser_fn)                         \
    Error* trampoline_fn(                                                      \
        const PdfObject* object,                                               \
        void* target_ptr,                                                      \
        PdfResolver* resolver                                                  \
    ) {                                                                        \
        return deser_fn(object, target_ptr, resolver);                         \
    }
