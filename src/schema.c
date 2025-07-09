#include <string.h>

#include "arena.h"
#include "pdf_object.h"
#include "pdf_result.h"
#include "pdf_schema.h"

// For-each macro
#define PP_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N
#define PP_NARG_(...) PP_ARG_N(__VA_ARGS__)
#define PP_NARG(...) PP_NARG_(__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

#define CAT(a, b) a##b

#define FOR_EACH_TUP_(N, f, ...) CAT(FOR_EACH_TUP_, N)(f, __VA_ARGS__)
#define FOR_EACH_TUP(f, ...) FOR_EACH_TUP_(PP_NARG(__VA_ARGS__), f, __VA_ARGS__)

#define FOR_EACH_TUP_1(f, x) f x
#define FOR_EACH_TUP_2(f, x, ...) f x FOR_EACH_TUP_1(f, __VA_ARGS__)
#define FOR_EACH_TUP_3(f, x, ...) f x FOR_EACH_TUP_2(f, __VA_ARGS__)
#define FOR_EACH_TUP_4(f, x, ...) f x FOR_EACH_TUP_3(f, __VA_ARGS__)
#define FOR_EACH_TUP_5(f, x, ...) f x FOR_EACH_TUP_4(f, __VA_ARGS__)
#define FOR_EACH_TUP_6(f, x, ...) f x FOR_EACH_TUP_5(f, __VA_ARGS__)
#define FOR_EACH_TUP_7(f, x, ...) f x FOR_EACH_TUP_6(f, __VA_ARGS__)
#define FOR_EACH_TUP_8(f, x, ...) f x FOR_EACH_TUP_7(f, __VA_ARGS__)
#define FOR_EACH_TUP_9(f, x, ...) f x FOR_EACH_TUP_8(f, __VA_ARGS__)
#define FOR_EACH_TUP_10(f, i, x, ...) f x FOR_EACH_TUP_9(f, __VA_ARGS__)

// Case conversions
#define SCHEMA_LOWERCASE_BOOLEAN boolean
#define SCHEMA_LOWERCASE_INTEGER integer
#define SCHEMA_LOWERCASE_REAL real
#define SCHEMA_LOWERCASE_STRING string
#define SCHEMA_LOWERCASE_NAME name
#define SCHEMA_LOWERCASE_ARRAY array
#define SCHEMA_LOWERCASE_DICT dict
#define SCHEMA_LOWERCASE_STREAM stream
#define SCHEMA_LOWERCASE_INDIRECT indirect
#define SCHEMA_LOWERCASE_REF ref

// Field initializers
#define SCHEMA_FIELD_INIT_BOOLEAN false
#define SCHEMA_FIELD_INIT_INTEGER 0
#define SCHEMA_FIELD_INIT_REAL 0.0
#define SCHEMA_FIELD_INIT_STRING ""
#define SCHEMA_FIELD_INIT_NAME ""
#define SCHEMA_FIELD_INIT_ARRAY NULL
#define SCHEMA_FIELD_INIT_DICT NULL
#define SCHEMA_FIELD_INIT_STREAM                                               \
    { .stream_dict = SCHEMA_FIELD_INIT_DICT, .stream_bytes = NULL }
#define SCHEMA_FIELD_INIT_INDIRECT                                             \
    { .object_id = 0, .generation = 0, .object = NULL }
#define SCHEMA_FIELD_INIT_REF                                                  \
    { .object_id = 0, .generation = 0 }

#define SCHEMA_FIELD_INIT_REQUIRED(name, type) .name = SCHEMA_FIELD_INIT_##type,
#define SCHEMA_FIELD_INIT_OPTIONAL(name, type)                                 \
    .name = {.has_value = false, .value = SCHEMA_FIELD_INIT_##type},
#define SCHEMA_FIELD_INIT(type, required, name, key)                           \
    SCHEMA_FIELD_INIT_##required(name, type)

// Field parsers
#define SCHEMA_FIELD_PARSE_REQUIRED(object_type, union_field, name, key)       \
    if (strcmp(entry_key, key) == 0) {                                         \
        if (object->kind != object_type) {                                     \
            *result = PDF_ERR_SCHEMA_INCORRECT_TYPE;                           \
            return NULL;                                                       \
        }                                                                      \
        if (name##_present) {                                                  \
            *result = PDF_ERR_SCHEMA_DUPLICATE_KEY;                            \
            return NULL;                                                       \
        }                                                                      \
        name##_present = true;                                                 \
        out.name = object->data.union_field;                                   \
    } else

#define SCHEMA_FIELD_PARSE_OPTIONAL(object_type, union_field, name, key)       \
    if (strcmp(entry_key, key) == 0) {                                         \
        if (object->kind != object_type) {                                     \
            *result = PDF_ERR_SCHEMA_INCORRECT_TYPE;                           \
            return NULL;                                                       \
        }                                                                      \
        if (name##_present) {                                                  \
            *result = PDF_ERR_SCHEMA_DUPLICATE_KEY;                            \
            return NULL;                                                       \
        }                                                                      \
        name##_present = true;                                                 \
        out.name.has_value = true;                                             \
        out.name.value = object->data.union_field;                             \
    } else

#define SCHEMA_FIELD_PARSE(type, required, name, key)                          \
    SCHEMA_FIELD_PARSE_##required(                                             \
        PDF_OBJECT_KIND_##type,                                                \
        SCHEMA_LOWERCASE_##type,                                               \
        name,                                                                  \
        key                                                                    \
    )

// Required restrictions
#define SCHEMA_FIELD_FLAG(type, required, name, key)                           \
    bool name##_present = false;

#define SCHEMA_CHECK_FIELD_FLAG_OPTIONAL(name)
#define SCHEMA_CHECK_FIELD_FLAG_REQUIRED(name)                                 \
    if (!name##_present) {                                                     \
        *result = PDF_ERR_MISSING_DICT_KEY;                                    \
        return NULL;                                                           \
    }
#define SCHEMA_CHECK_FIELD_FLAG(type, required, name, key)                     \
    SCHEMA_CHECK_FIELD_FLAG_##required(name)

// Implementations
#define SCHEMA_IMPL(type_name, snake_name, ...)                                \
    type_name* pdf_schema_##snake_name##_new(                                  \
        Arena* arena,                                                          \
        PdfObject* dict,                                                       \
        PdfResult* result                                                      \
    ) {                                                                        \
        if (!arena || !dict || !result) {                                      \
            return NULL;                                                       \
        }                                                                      \
        *result = PDF_OK;                                                      \
        if (dict->kind != PDF_OBJECT_KIND_DICT) {                              \
            *result = PDF_ERR_OBJECT_NOT_DICT;                                 \
            return NULL;                                                       \
        }                                                                      \
        type_name out = {FOR_EACH_TUP(SCHEMA_FIELD_INIT, __VA_ARGS__)};        \
        FOR_EACH_TUP(SCHEMA_FIELD_FLAG, __VA_ARGS__)                           \
        for (size_t entry_idx = 0; entry_idx < vec_len(dict->data.dict);       \
             entry_idx++) {                                                    \
            PdfObjectDictEntry* entry = vec_get(dict->data.dict, entry_idx);   \
            char* entry_key = entry->key->data.name;                           \
            PdfObject* object = entry->value;                                  \
            if (!entry_key || !object) {                                       \
                return NULL;                                                   \
            } else                                                             \
                FOR_EACH_TUP(SCHEMA_FIELD_PARSE, __VA_ARGS__) {                \
                    *result = PDF_ERR_SCHEMA_UNKNOWN_KEY;                      \
                    return NULL;                                               \
                }                                                              \
        }                                                                      \
        FOR_EACH_TUP(SCHEMA_CHECK_FIELD_FLAG, __VA_ARGS__)                     \
        type_name* out_ptr = arena_alloc(arena, sizeof(type_name));            \
        *out_ptr = out;                                                        \
        return out_ptr;                                                        \
    }

SCHEMA_IMPL(
    PdfSchemaTrailer,
    trailer,
    (INTEGER, REQUIRED, size, "Size"),
    (INTEGER, OPTIONAL, prev, "Prev"),
    (REF, REQUIRED, root, "Root"),
    (DICT, OPTIONAL, encrypt, "Encrypt"),
    (DICT, OPTIONAL, info, "Info"),
    (ARRAY, OPTIONAL, id, "Id")
)
