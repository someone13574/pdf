#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "arena.h"
#include "ctx.h"
#include "log.h"
#include "object.h"
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
#define OBJECT_UNION_FIELD_BOOLEAN boolean
#define OBJECT_UNION_FIELD_INTEGER integer
#define OBJECT_UNION_FIELD_REAL real
#define OBJECT_UNION_FIELD_STRING string
#define OBJECT_UNION_FIELD_NAME name
#define OBJECT_UNION_FIELD_ARRAY array
#define OBJECT_UNION_FIELD_DICT dict
#define OBJECT_UNION_FIELD_STREAM stream
#define OBJECT_UNION_FIELD_INDIRECT indirect
#define OBJECT_UNION_FIELD_REF ref

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

#define SCHEMA_FIELD_INIT_REQUIRED(field_name, field_type)                     \
    .field_name = SCHEMA_FIELD_INIT_##field_type,
#define SCHEMA_FIELD_INIT_OPTIONAL(field_name, field_type)                     \
    .field_name = {.has_value = false, .value = SCHEMA_FIELD_INIT_##field_type},
#define SCHEMA_FIELD_INIT_REQUIRED_SCHEMA_REF(name, schema_name)               \
    .name = {                                                                  \
        .get = &pdf_schema_##schema_name##_get_ref,                            \
        .ref = SCHEMA_FIELD_INIT_REF,                                          \
        .cached = NULL                                                         \
    },
#define SCHEMA_FIELD_INIT_OPTIONAL_SCHEMA_REF(name, schema_name)               \
    .name = {                                                                  \
        .has_value = false,                                                    \
        .value =                                                               \
            {.get = pdf_schema_##schema_name##_get_ref,                        \
             .ref = SCHEMA_FIELD_INIT_REF,                                     \
             .cached = NULL}                                                   \
    },
#define SCHEMA_FIELD_INIT(required, field_type, field_name, key)               \
    SCHEMA_FIELD_INIT_##required(field_name, field_type)

// Field parsers
#define SCHEMA_FIELD_PARSE_REQUIRED(object_type, name, key)                    \
    if (strcmp(entry_key, key) == 0) {                                         \
        if (entry_value->kind != PDF_OBJECT_KIND_##object_type) {              \
            *result = PDF_ERR_SCHEMA_INCORRECT_TYPE;                           \
            LOG_TRACE_G(                                                       \
                "schema",                                                      \
                "Schema key `" key "` has incorrect type %d",                  \
                entry_value->kind                                              \
            );                                                                 \
            return NULL;                                                       \
        }                                                                      \
        if (name##_present) {                                                  \
            *result = PDF_ERR_SCHEMA_DUPLICATE_KEY;                            \
            return NULL;                                                       \
        }                                                                      \
        name##_present = true;                                                 \
        out.name = object->data.OBJECT_UNION_FIELD_##object_type;              \
    } else

#define SCHEMA_FIELD_PARSE_OPTIONAL(object_type, name, key)                    \
    if (strcmp(entry_key, key) == 0) {                                         \
        if (entry_value->kind != PDF_OBJECT_KIND_##object_type) {              \
            *result = PDF_ERR_SCHEMA_INCORRECT_TYPE;                           \
            LOG_TRACE_G(                                                       \
                "schema",                                                      \
                "Schema key `" key "` has incorrect type %d",                  \
                entry_value->kind                                              \
            );                                                                 \
            return NULL;                                                       \
        }                                                                      \
        if (name##_present) {                                                  \
            *result = PDF_ERR_SCHEMA_DUPLICATE_KEY;                            \
            return NULL;                                                       \
        }                                                                      \
        name##_present = true;                                                 \
        out.name.has_value = true;                                             \
        out.name.value = entry_value->data.OBJECT_UNION_FIELD_##object_type;   \
    } else

#define SCHEMA_FIELD_PARSE_REQUIRED_SCHEMA_REF(object_type, name, key)         \
    if (strcmp(entry_key, key) == 0) {                                         \
        if (entry_value->kind != PDF_OBJECT_KIND_REF) {                        \
            *result = PDF_ERR_SCHEMA_INCORRECT_TYPE;                           \
            LOG_TRACE_G(                                                       \
                "schema",                                                      \
                "Schema key `" key "` has incorrect type %d",                  \
                entry_value->kind                                              \
            );                                                                 \
            return NULL;                                                       \
        }                                                                      \
        if (name##_present) {                                                  \
            *result = PDF_ERR_SCHEMA_DUPLICATE_KEY;                            \
            return NULL;                                                       \
        }                                                                      \
        name##_present = true;                                                 \
        out.name.ref = entry_value->data.OBJECT_UNION_FIELD_REF;               \
    } else

#define SCHEMA_FIELD_PARSE_OPTIONAL_SCHEMA_REF(object_type, name, key)         \
    if (strcmp(entry_key, key) == 0) {                                         \
        if (entry_value->kind != PDF_OBJECT_KIND_REF) {                        \
            *result = PDF_ERR_SCHEMA_INCORRECT_TYPE;                           \
            LOG_TRACE_G(                                                       \
                "schema",                                                      \
                "Schema key `" key "` has incorrect type %d",                  \
                entry_value->kind                                              \
            );                                                                 \
            return NULL;                                                       \
        }                                                                      \
        if (name##_present) {                                                  \
            *result = PDF_ERR_SCHEMA_DUPLICATE_KEY;                            \
            return NULL;                                                       \
        }                                                                      \
        name##_present = true;                                                 \
        out.name.has_value = true;                                             \
        out.name.value.ref = entry_value->data.OBJECT_UNION_FIELD_REF;         \
    } else

#define SCHEMA_FIELD_PARSE(required, type, field_name, key)                    \
    SCHEMA_FIELD_PARSE_##required(type, field_name, key)

// Required restrictions
#define SCHEMA_FIELD_FLAG(required, type, field_name, key)                     \
    bool field_name##_present = false;

#define SCHEMA_CHECK_FIELD_FLAG_REQUIRED(field_name)                           \
    if (!field_name##_present) {                                               \
        *result = PDF_ERR_MISSING_DICT_KEY;                                    \
        return NULL;                                                           \
    }
#define SCHEMA_CHECK_FIELD_FLAG_OPTIONAL(field_name)
#define SCHEMA_CHECK_FIELD_FLAG_REQUIRED_SCHEMA_REF(field_name)                \
    if (!field_name##_present) {                                               \
        *result = PDF_ERR_MISSING_DICT_KEY;                                    \
        return NULL;                                                           \
    }
#define SCHEMA_CHECK_FIELD_FLAG_OPTIONAL_SCHEMA_REF(field_name)
#define SCHEMA_CHECK_FIELD_FLAG(required, type, field_name, key)               \
    SCHEMA_CHECK_FIELD_FLAG_##required(field_name)

// Implementations
#define SCHEMA_IMPL(type_name, snake_name, ...)                                \
    type_name* pdf_schema_##snake_name##_new(                                  \
        Arena* arena,                                                          \
        PdfObject* object,                                                     \
        PdfResult* result                                                      \
    ) {                                                                        \
        if (!arena || !object || !result) {                                    \
            return NULL;                                                       \
        }                                                                      \
        *result = PDF_OK;                                                      \
        LOG_DEBUG_G("schema", "Creating schema " #type_name " from object");   \
        PdfObject* dict = NULL;                                                \
        if (object->kind == PDF_OBJECT_KIND_INDIRECT) {                        \
            dict = object->data.indirect.object;                               \
        } else {                                                               \
            dict = object;                                                     \
        }                                                                      \
        if (dict->kind != PDF_OBJECT_KIND_DICT) {                              \
            LOG_TRACE_G("schema", "Object was type %d, not dict", dict->kind); \
            *result = PDF_ERR_OBJECT_NOT_DICT;                                 \
            return NULL;                                                       \
        }                                                                      \
        type_name out = {                                                      \
            FOR_EACH_TUP(SCHEMA_FIELD_INIT, __VA_ARGS__).dict = dict           \
        };                                                                     \
        FOR_EACH_TUP(SCHEMA_FIELD_FLAG, __VA_ARGS__)                           \
        for (size_t entry_idx = 0; entry_idx < vec_len(dict->data.dict);       \
             entry_idx++) {                                                    \
            PdfObjectDictEntry* entry = vec_get(dict->data.dict, entry_idx);   \
            char* entry_key = entry->key->data.name;                           \
            PdfObject* entry_value = entry->value;                             \
            if (!entry_key || !entry_value) {                                  \
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

#define SCHEMA_IMPL_GET_REF(schema_type, schema_name)                          \
    schema_type* pdf_schema_##schema_name##_get_ref(                           \
        schema_type##Ref* ref,                                                 \
        PdfDocument* doc,                                                      \
        PdfResult* result                                                      \
    ) {                                                                        \
        if (!ref || !doc || !result) {                                         \
            return NULL;                                                       \
        }                                                                      \
        LOG_DEBUG_G("schema", "Resolving schema reference to " #schema_type);  \
        *result = PDF_OK;                                                      \
        if (ref->cached) {                                                     \
            LOG_TRACE_G("schema", "Using cached value");                       \
            return ref->cached;                                                \
        }                                                                      \
        PdfObject* object = pdf_get_ref(doc, ref->ref, result);                \
        if (*result != PDF_OK || !object) {                                    \
            return NULL;                                                       \
        }                                                                      \
        ref->cached = pdf_schema_##schema_name##_new(                          \
            pdf_doc_arena(doc),                                                \
            object,                                                            \
            result                                                             \
        );                                                                     \
        return ref->cached;                                                    \
    }

SCHEMA_IMPL(
    PdfSchemaCatalog,
    catalog,
    (REQUIRED, NAME, type, "Type"),
    (REQUIRED, REF, pages, "Pages")
)

SCHEMA_IMPL(
    PdfSchemaTrailer,
    trailer,
    (REQUIRED, INTEGER, size, "Size"),
    (REQUIRED_SCHEMA_REF, catalog, root, "Root")
)

SCHEMA_IMPL_GET_REF(PdfSchemaCatalog, catalog)

#ifdef TEST

#include "test.h"

TEST_FUNC(test_trailer_schema) {
    char buffer[] = "<</Size 5 /Root 1 0 R >>";
    Arena* arena = arena_new(128);
    PdfCtx* ctx = pdf_ctx_new(arena, buffer, strlen(buffer));
    PdfResult result = PDF_OK;
    PdfObject* object = pdf_parse_object(arena, ctx, &result, false);
    TEST_ASSERT_EQ((PdfResult)PDF_OK, result);
    TEST_ASSERT(object);

    PdfSchemaTrailer* trailer = pdf_schema_trailer_new(arena, object, &result);
    TEST_ASSERT_EQ((PdfResult)PDF_OK, result);
    TEST_ASSERT(trailer);

    TEST_ASSERT_EQ(trailer->size, 5);
    TEST_ASSERT_EQ(
        (uintptr_t)trailer->root.get,
        (uintptr_t)pdf_schema_catalog_get_ref
    );
    TEST_ASSERT_EQ(trailer->root.ref.object_id, (size_t)1);
    TEST_ASSERT_EQ(trailer->root.ref.generation, (size_t)0);
    TEST_ASSERT(!trailer->root.cached);

    return TEST_RESULT_PASS;
}

#endif // TEST
