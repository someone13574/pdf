#include <string.h>

#include "arena.h"
#include "log.h"
#include "pdf_doc.h"
#include "pdf_object.h"
#include "pdf_result.h"
#include "vec.h"

#ifndef SCHEMA_IMPL_H
#define SCHEMA_IMPL_H

typedef enum { SCHEMA_FC_OBJECT, SCHEMA_FC_SCHEMA_REF } SchemaFieldClass;

bool schema_validation_none(void* schema, PdfResult* result) {
    (void)schema;
    *result = PDF_OK;
    return true;
}

#endif // SCHEMA_IMPL_H

// Check for parameters
#ifndef SCHEMA_NAME
#error "SCHEMA_NAME not defined"
#define SCHEMA_NAME PdfSchemaPlaceholder
#define PLACEHOLDER
#endif

#ifndef SCHEMA_LOWERCASE_NAME
#error "SCHEMA_LOWERCASE_NAME not defined"
#define SCHEMA_LOWERCASE_NAME pdf_schema_placeholder
#define PLACEHOLDER
#endif

#ifndef SCHEMA_FIELDS
#error "SCHEMA_FIELDS not defined"
#define SCHEMA_FIELDS
#define PLACEHOLDER
#endif

#ifndef SCHEMA_VALIDATION_FN
#error "SCHEMA_VALIDATION_FN not defined"
#define SCHEMA_VALIDATION_FN schema_validation_none
#endif

#define STRINGIFY_INNER(x) #x
#define STRINGIFY(x) STRINGIFY_INNER(x)
#define PASTE_INNER(a, b) a##b
#define PASTE(a, b) PASTE_INNER(a, b)

// Define names
#define REF_NAME PASTE(SCHEMA_NAME, Ref)
#define OP_REF_NAME PASTE(SCHEMA_NAME, OpRef)

#define NEW_FN PASTE(SCHEMA_LOWERCASE_NAME, _new)
#define RESOLVE_FN PASTE(SCHEMA_LOWERCASE_NAME, _resolve)

// Placeholder declarations to make lsp happy
#ifdef PLACEHOLDER
typedef struct {
    PdfObject* raw_dict;
} SCHEMA_NAME;

typedef struct REF_NAME {
    SCHEMA_NAME* (*resolve)(
        struct REF_NAME* ref,
        PdfDocument* doc,
        PdfResult* result
    );
    PdfObjectRef ref_object;
    SCHEMA_NAME* cached;
} REF_NAME;

typedef struct OP_REF_NAME {
    bool has_value;
    REF_NAME value;
} OP_REF_NAME;
#endif // PLACEHOLDER

// Deserialization helpers
#define DESERDE_SCHEMA_FC_OBJECT_REQUIRED(field_name, type, accessor)          \
    if (entry->value->kind != (type)) {                                        \
        *result = PDF_ERR_SCHEMA_INCORRECT_TYPE;                               \
        return NULL;                                                           \
    }                                                                          \
    schema.field_name = entry->value->data.accessor;

#define DESERDE_SCHEMA_FC_SCHEMA_REF_REQUIRED(field_name, type, accessor)      \
    if (entry->value->kind != PDF_OBJECT_KIND_REF) {                           \
        *result = PDF_ERR_SCHEMA_INCORRECT_TYPE;                               \
        return NULL;                                                           \
    }                                                                          \
    schema.field_name.resolve = accessor;                                      \
    schema.field_name.ref_object = entry->value->data.ref;                     \
    schema.field_name.cached = NULL;

#define DESERDE_SCHEMA_FC_SCHEMA_REF_OPTIONAL(field_name, type, accessor)      \
    if (entry->value->kind != PDF_OBJECT_KIND_REF) {                           \
        *result = PDF_ERR_SCHEMA_INCORRECT_TYPE;                               \
        return NULL;                                                           \
    }                                                                          \
    schema.field_name.has_value = true;                                        \
    schema.field_name.value.resolve = accessor;                                \
    schema.field_name.value.ref_object = entry->value->data.ref;               \
    schema.field_name.value.cached = NULL;

#define CHECK_FIELD_REQUIRED(field_name)                                       \
    if (!field_name##_present) {                                               \
        *result = PDF_ERR_MISSING_DICT_KEY;                                    \
        return NULL;                                                           \
    }

#define CHECK_FIELD_OPTIONAL(field_name)

// Deserialization function
SCHEMA_NAME* NEW_FN(PdfObject* object, Arena* arena, PdfResult* result) {
    if (!object || !arena || !result) {
        return NULL;
    }

    LOG_DEBUG_G(
        "schema",
        "Deserializing object of type %d into " STRINGIFY(SCHEMA_NAME),
        object->kind
    );
    *result = PDF_OK;

    // Handle indirect objects
    if (object->kind == PDF_OBJECT_KIND_INDIRECT) {
        object = object->data.indirect.object;
    }

    if (object->kind != PDF_OBJECT_KIND_DICT) {
        *result = PDF_ERR_OBJECT_NOT_DICT;
        return NULL;
    }

    // Deserialize fields
    SCHEMA_NAME schema = {.raw_dict = object};

#define X(key, field_name, required, field_class, type, accessor)              \
    bool field_name##_present = false;
    SCHEMA_FIELDS
#undef X

    for (size_t entry_idx = 0; entry_idx < vec_len(object->data.dict);
         entry_idx++) {
        PdfObjectDictEntry* entry = vec_get(object->data.dict, entry_idx);
        if (!entry) {
            return NULL;
        }

        LOG_TRACE_G("schema", "Deserializing key `%s`", entry->key->data.name);

#define X(field_key, field_name, required, field_class, type, accessor)        \
    if (strcmp(entry->key->data.name, field_key) == 0) {                       \
        if (field_name##_present) {                                            \
            *result = PDF_ERR_SCHEMA_DUPLICATE_KEY;                            \
            return NULL;                                                       \
        }                                                                      \
        field_name##_present = true;                                           \
        DESERDE_##field_class##_##required(                                    \
            field_name,                                                        \
            type,                                                              \
            accessor                                                           \
        ) continue;                                                            \
    }
        SCHEMA_FIELDS
#undef X

        *result = PDF_ERR_SCHEMA_UNKNOWN_KEY;
        LOG_WARN_G("schema", "Unknown schema key `%s`", entry->key->data.name);
        return NULL;
    }

#define X(key, field_name, required, field_class, type, accessor)              \
    CHECK_FIELD_##required(field_name)
    SCHEMA_FIELDS
#undef X

    if (!SCHEMA_VALIDATION_FN(&schema, result) || *result != PDF_OK) {
        return NULL;
    }

    SCHEMA_NAME* schema_ptr = arena_alloc(arena, sizeof(SCHEMA_NAME));
    *schema_ptr = schema;
    return schema_ptr;
}

// Resolve function
SCHEMA_NAME* RESOLVE_FN(REF_NAME* ref, PdfDocument* doc, PdfResult* result) {
    if (!ref || !doc || !result) {
        return NULL;
    }

    LOG_DEBUG_G("schema", "Resolving " STRINGIFY(REF_NAME));
    *result = PDF_OK;

    // Use cached value if available
    if (ref->cached) {
        return NULL;
    }

    // Resolve object
    PdfObject* object = pdf_get_ref(doc, ref->ref_object, result);
    if (*result != PDF_OK || !object) {
        return NULL;
    }

    // Deserialize object
    ref->cached = NEW_FN(object, pdf_doc_arena(doc), result);
    return ref->cached;
}

// Undefs
#undef SCHEMA_NAME
#undef PLACEHOLDER
#undef SCHEMA_LOWERCASE_NAME
#undef SCHEMA_FIELDS
#undef SCHEMA_VALIDATION_FN
#undef STRINGIFY
#undef STRINGIFY_INNER
#undef PASTE
#undef PASTE_INNER
#undef REF_NAME
#undef OP_REF_NAME
#undef NEW_FN
#undef RESOLVE_FN
#undef DESERDE_SCHEMA_FC_OBJECT_REQUIRED
#undef DESERDE_SCHEMA_FC_SCHEMA_REF_REQUIRED
#undef CHECK_FIELD_REQUIRED
#undef CHECK_FIELD_OPTIONAL
