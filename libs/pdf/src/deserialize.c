#include "deserialize.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "arena/arena.h"
#include "logger/log.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"

static PdfError* deserialize(
    const PdfObject* object,
    void* target_ptr,
    PdfDeserdeInfo deserde_info,
    PdfResolver* resolver
);

static PdfError* deserialize_primitive(
    const PdfObject* object,
    void* target_ptr,
    PdfObjectType object_type,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    PdfObject resolved_object;
    if (object->type != object_type) {
        PDF_PROPAGATE(pdf_resolve_object(
            resolver,
            object,
            &resolved_object,
            object_type != PDF_OBJECT_TYPE_INDIRECT_OBJECT
        ));
    } else {
        resolved_object = *object;
    }

    LOG_DIAG(TRACE, DESERDE, "Deserializing primitive object");

    if (resolved_object.type != object_type) {
        return PDF_ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "Incorrect type for primitive object. Expected `%d`, got `%d`",
            object_type,
            resolved_object.type
        );
    }

    switch (object_type) {
        case PDF_OBJECT_TYPE_BOOLEAN: {
            *(PdfBoolean*)target_ptr = resolved_object.data.boolean;
            break;
        }
        case PDF_OBJECT_TYPE_INTEGER: {
            *(PdfInteger*)target_ptr = resolved_object.data.integer;
            break;
        }
        case PDF_OBJECT_TYPE_REAL: {
            *(PdfReal*)target_ptr = resolved_object.data.real;
            break;
        }
        case PDF_OBJECT_TYPE_STRING: {
            *(PdfString*)target_ptr = resolved_object.data.string;
            break;
        }
        case PDF_OBJECT_TYPE_NAME: {
            *(PdfName*)target_ptr = resolved_object.data.name;
            break;
        }
        case PDF_OBJECT_TYPE_ARRAY: {
            *(PdfArray*)target_ptr = resolved_object.data.array;
            break;
        }
        case PDF_OBJECT_TYPE_DICT: {
            *(PdfDict*)target_ptr = resolved_object.data.dict;
            break;
        }
        case PDF_OBJECT_TYPE_STREAM: {
            *(PdfStream*)target_ptr = resolved_object.data.stream;
            break;
        }
        case PDF_OBJECT_TYPE_INDIRECT_OBJECT: {
            *(PdfIndirectObject*)target_ptr =
                resolved_object.data.indirect_object;
            break;
        }
        case PDF_OBJECT_TYPE_INDIRECT_REF: {
            *(PdfIndirectRef*)target_ptr = resolved_object.data.indirect_ref;
            break;
        }
        case PDF_OBJECT_TYPE_NULL: {
            break;
        }
    }

    return NULL;
}

static PdfError* deserialize_optional(
    const PdfObject* object,
    void* target_ptr,
    PdfDeserdeOptionalInfo deserde_info,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(deserde_info.init_optional);
    RELEASE_ASSERT(resolver);

    LOG_DIAG(TRACE, DESERDE, "Deserializing optional");

    // Initializes the optional and gets a pointer to it's inner field
    void* value_ptr = deserde_info.init_optional(target_ptr, true);

    PDF_PROPAGATE(deserialize(
        object,
        value_ptr,
        *deserde_info.inner_deserde_info,
        resolver
    ));

    return NULL;
}

static void
set_none_optional(void* target_ptr, PdfDeserdeOptionalInfo deserde_info) {
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(deserde_info.init_optional);

    deserde_info.init_optional(target_ptr, false);
}

static PdfError* deserialize_resolvable(
    const PdfObject* object,
    void* target_ptr,
    PdfDeserdeInitResolvable init_fn
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(init_fn);

    LOG_DIAG(TRACE, DESERDE, "Deserializing resolvable");

    if (object->type != PDF_OBJECT_TYPE_INDIRECT_REF) {
        return PDF_ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "Incorrect type for deserializing resolvable type. Expected an indirect reference, got type `%d`",
            (int)object->type
        );
    }

    init_fn(target_ptr, object->data.indirect_ref);

    return NULL;
}

static PdfError* deserialize_array(
    const PdfObject* object,
    void* target_ptr, // Represents a pointer to the vec pointer
    PdfDeserdeArrayInfo deserde_info,
    PdfResolver* resolver,
    void* first_element_ptr // When deserializing 'as-array' types, it will
                            // first attempt to deserialize the singular
                            // unwrapped element, but this can fail and we don't
                            // have a way to clear the array, so we will need to
                            // reuse the allocated element.
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(deserde_info.vec_push_uninit);
    RELEASE_ASSERT(resolver);

    LOG_DIAG(TRACE, DESERDE, "Deserializing array");

    PdfObject resolved_object;
    PDF_PROPAGATE(pdf_resolve_object(resolver, object, &resolved_object, true));

    if (resolved_object.type != PDF_OBJECT_TYPE_ARRAY) {
        return PDF_ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "Incorrect type for deserializing array. Expected an array, got type `%d`",
            (int)resolved_object.type
        );
    }

    if (pdf_object_vec_len(resolved_object.data.array.elements) == 0
        && first_element_ptr) {
        LOG_TODO(
            "Implement popping of first_element_ptr when deserializing an as-array with zero elements"
        );
    }

    for (size_t idx = 0;
         idx < pdf_object_vec_len(resolved_object.data.array.elements);
         idx++) {
        PdfObject* element;
        RELEASE_ASSERT(pdf_object_vec_get(
            resolved_object.data.array.elements,
            idx,
            &element
        ));

        // This pointer should be sized and aligned correctly since it was
        // initialized by the vector, which knows the underlying type.
        void* element_target_ptr = NULL;
        if (first_element_ptr) {
            // See note above about `first_element_ptr`.
            element_target_ptr = first_element_ptr;
            first_element_ptr = NULL;
        } else {
            if (idx == 0) {
                *(void**)target_ptr = NULL;
            }

            element_target_ptr = deserde_info.vec_push_uninit(
                target_ptr,
                pdf_resolver_arena(resolver)
            );
        }

        PDF_PROPAGATE(
            deserialize(
                element,
                element_target_ptr,
                *deserde_info.element_deserde_info,
                resolver
            ),
            "While deserializing array element"
        );
    }

    return NULL;
}

// This function differs from normal array deserialization since it allows
// singular elements which aren't in an array to be treated as if they were.
static PdfError* deserialize_as_array(
    const PdfObject* object,
    void* target_ptr, // represents a pointer to the vec pointer
    PdfDeserdeArrayInfo deserde_info,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(deserde_info.vec_push_uninit);
    RELEASE_ASSERT(resolver);

    LOG_DIAG(TRACE, DESERDE, "Deserializing as-array");

    *(void**)target_ptr = NULL;

    // Attempt unwrapped-element deserialization first
    void* element_target_ptr =
        deserde_info.vec_push_uninit(target_ptr, pdf_resolver_arena(resolver));
    PdfError* unwrapped_deserde_err = deserialize(
        object,
        element_target_ptr,
        *deserde_info.element_deserde_info,
        resolver
    );

    if (!unwrapped_deserde_err) {
        LOG_DIAG(TRACE, DESERDE, "Deserialized single element as array");
        return NULL;
    }

    LOG_DIAG(TRACE, DESERDE, "Falling-back to array deserialization");

    // Fall-back to normal array deserialization. Reuse allocated
    // `element_target_ptr`.
    PDF_PROPAGATE(pdf_error_conditional_context(
        deserialize_array(
            object,
            target_ptr,
            deserde_info,
            resolver,
            element_target_ptr
        ),
        unwrapped_deserde_err
    ));

    return NULL;
}

static PdfError* deserialize(
    const PdfObject* object,
    void* target_ptr,
    PdfDeserdeInfo deserde_info,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(deserde_info.type != PDF_DESERDE_TYPE_UNIMPLEMENTED);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    switch (deserde_info.type) {
        case PDF_DESERDE_TYPE_UNIMPLEMENTED:
        case PDF_DESERDE_TYPE_IGNORED: {
            LOG_PANIC("Unreachable");
        }
        case PDF_DESERDE_TYPE_OBJECT: {
            PDF_PROPAGATE(deserialize_primitive(
                object,
                target_ptr,
                deserde_info.value.object_info,
                resolver
            ));
            break;
        }
        case PDF_DESERDE_TYPE_OPTIONAL: {
            PDF_PROPAGATE(deserialize_optional(
                object,
                target_ptr,
                deserde_info.value.optional_info,
                resolver
            ));
            break;
        }
        case PDF_DESERDE_TYPE_RESOLVABLE: {
            PDF_PROPAGATE(deserialize_resolvable(
                object,
                target_ptr,
                deserde_info.value.resolvable_info
            ));
            break;
        }
        case PDF_DESERDE_TYPE_ARRAY: {
            PDF_PROPAGATE(deserialize_array(
                object,
                target_ptr,
                deserde_info.value.array_info,
                resolver,
                NULL
            ));
            break;
        }
        case PDF_DESERDE_TYPE_AS_ARRAY: {
            PDF_PROPAGATE(deserialize_as_array(
                object,
                target_ptr,
                deserde_info.value.array_info,
                resolver
            ));
            break;
        }
        case PDF_DESERDE_TYPE_CUSTOM: {
            PDF_PROPAGATE(
                deserde_info.value.custom_fn(object, target_ptr, resolver)
            );
            break;
        }
    }

    return NULL;
}

PdfError* pdf_deserialize_dict(
    const PdfObject* object,
    const PdfFieldDescriptor* fields,
    size_t num_fields,
    bool allow_unknown_fields,
    PdfResolver* resolver,
    const char* debug_name
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(fields);
    RELEASE_ASSERT(resolver);

    // Resolve object
    PdfObject resolved_object;
    PDF_PROPAGATE(pdf_resolve_object(resolver, object, &resolved_object, true));

    // Check object type
    if (resolved_object.type != PDF_OBJECT_TYPE_DICT) {
        return PDF_ERROR(
            PDF_ERR_OBJECT_NOT_DICT,
            "Object type is not a dictionary"
        );
    }

    LOG_DIAG(
        INFO,
        DESERDE,
        "Deserializing dictionary object `%s` (allow_unknown_fields=%s)",
        debug_name ? debug_name : "(no name provided)",
        allow_unknown_fields ? "true" : "false"
    );

    // Reject unknown keys
    if (!allow_unknown_fields) {
        for (size_t entry_idx = 0;
             entry_idx
             < pdf_dict_entry_vec_len(resolved_object.data.dict.entries);
             entry_idx++) {
            bool found = false;

            PdfDictEntry entry;
            pdf_dict_entry_vec_get(
                resolved_object.data.dict.entries,
                entry_idx,
                &entry
            );

            for (size_t field_idx = 0; field_idx < num_fields; field_idx++) {
                if (strcmp(entry.key->data.name, fields[field_idx].key) == 0) {
                    if (found) {
                        return PDF_ERROR(
                            PDF_ERR_DUPLICATE_KEY,
                            "Duplicate dict key `%s`",
                            entry.key->data.name
                        );
                    }

                    found = true;
                }
            }

            if (!found) {
                return PDF_ERROR(
                    PDF_ERR_UNKNOWN_KEY,
                    "Dict key `%s` is not a known field",
                    entry.key->data.name
                );
            }
        }
    }

    // Deserialize fields
    for (size_t field_idx = 0; field_idx < num_fields; field_idx++) {
        bool found = false;
        const PdfFieldDescriptor* field = &fields[field_idx];
        LOG_DIAG(
            DEBUG,
            DESERDE,
            "Field: `%s` (\x1b[4m%s:%lu\x1b[0m)",
            field->key,
            field->debug_info.file,
            field->debug_info.line
        );

        for (size_t entry_idx = 0;
             entry_idx
             < pdf_dict_entry_vec_len(resolved_object.data.dict.entries);
             entry_idx++) {
            PdfDictEntry entry;
            pdf_dict_entry_vec_get(
                resolved_object.data.dict.entries,
                entry_idx,
                &entry
            );
            if (strcmp(entry.key->data.name, fields[field_idx].key) != 0) {
                continue;
            }

            if (field->deserde_info.type == PDF_DESERDE_TYPE_UNIMPLEMENTED) {
                LOG_PANIC(
                    "Unimplemented field `%s` (\x1b[4m%s:%lu\x1b[0m)",
                    field->key,
                    field->debug_info.file,
                    field->debug_info.line
                );
            }

            if (field->deserde_info.type == PDF_DESERDE_TYPE_IGNORED) {
                continue;
            }

            PDF_PROPAGATE(
                deserialize(
                    entry.value,
                    field->target_ptr,
                    field->deserde_info,
                    resolver
                ),
                "Error while deserializing field `%s` (\x1b[4m%s:%lu\x1b[0m)",
                field->key,
                field->debug_info.file,
                field->debug_info.line
            );

            found = true;
            break;
        }

        if (!found) {
            if (field->deserde_info.type == PDF_DESERDE_TYPE_OPTIONAL) {
                set_none_optional(
                    field->target_ptr,
                    field->deserde_info.value.optional_info
                );
            } else if (field->deserde_info.type
                           != PDF_DESERDE_TYPE_UNIMPLEMENTED
                       && field->deserde_info.type
                              != PDF_DESERDE_TYPE_IGNORED) {
                return PDF_ERROR(
                    PDF_ERR_MISSING_DICT_KEY,
                    "Missing key `%s`",
                    field->key
                );
            }
        }
    }

    LOG_DIAG(TRACE, DESERDE, "Finished deserializing dictionary object");

    return NULL;
}

PdfError* pdf_deserialize_operands(
    const PdfObjectVec* operands,
    const PdfOperandDescriptor* descriptors,
    size_t num_descriptors,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(descriptors);

    if (num_descriptors > pdf_object_vec_len(operands)) {
        return PDF_ERROR(
            PDF_ERR_MISSING_OPERAND,
            "Incorrect number of operands. Expected %zu, found %zu",
            num_descriptors,
            pdf_object_vec_len(operands)
        );
    }

    if (num_descriptors < pdf_object_vec_len(operands)) {
        return PDF_ERROR(
            PDF_ERR_EXCESS_OPERAND,
            "Incorrect number of operands. Expected %zu, found %zu",
            num_descriptors,
            pdf_object_vec_len(operands)
        );
    }

    for (size_t idx = 0; idx < num_descriptors; idx++) {
        PdfOperandDescriptor descriptor = descriptors[idx];

        PdfObject* operand = NULL;
        RELEASE_ASSERT(pdf_object_vec_get(operands, idx, &operand));

        PDF_PROPAGATE(deserialize(
            operand,
            descriptor.target_ptr,
            descriptor.deserde_info,
            resolver
        ));
    }

    return NULL;
}

// TODO: Select less-terrible names for the types in these tests
#ifdef TEST

#include "object.h"
#include "test/test.h"
#include "test_helpers.h"

#define DESERIALIZER_IMPL_HELPER()                                             \
    do {                                                                       \
        PDF_PROPAGATE(pdf_deserialize_dict(                                    \
            object,                                                            \
            fields,                                                            \
            sizeof(fields) / sizeof(PdfFieldDescriptor),                       \
            false,                                                             \
            resolver,                                                          \
            "test-object"                                                      \
        ));                                                                    \
        return NULL;                                                           \
    } while (0)

#define DESERIALIZER_TEST_HELPER()                                             \
    PdfResolver* resolver;                                                     \
    TEST_PDF_REQUIRE(                                                          \
        pdf_resolver_new(arena, (uint8_t*)buffer, strlen(buffer), &resolver)   \
    );                                                                         \
    PdfObject object;                                                          \
    TEST_PDF_REQUIRE(pdf_resolve_ref(                                          \
        resolver,                                                              \
        (PdfIndirectRef) {.object_id = 1, .generation = 0},                    \
        &object                                                                \
    ));

typedef struct {
    PdfBoolean boolean;
    PdfInteger integer;
    PdfReal real;
    PdfString string;
    PdfName name;
    PdfArray array;
    PdfDict dict;
    PdfStream stream;
    PdfIndirectRef indirect_ref;
} TestDeserializeObjectsStruct;

PdfError* deserialize_test_objects_struct(
    PdfObject* object,
    TestDeserializeObjectsStruct* deserialized,
    PdfResolver* resolver
) {
    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            "Boolean",
            &deserialized->boolean,
            PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_BOOLEAN)
        ),
        PDF_FIELD(
            "Integer",
            &deserialized->integer,
            PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_INTEGER)
        ),
        PDF_FIELD(
            "Real",
            &deserialized->real,
            PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_REAL)
        ),
        PDF_FIELD(
            "String",
            &deserialized->string,
            PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_STRING)
        ),
        PDF_FIELD(
            "Name",
            &deserialized->name,
            PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(
            "Array",
            &deserialized->array,
            PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_ARRAY)
        ),
        PDF_FIELD(
            "Dict",
            &deserialized->dict,
            PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_DICT)
        ),
        PDF_FIELD(
            "Stream",
            &deserialized->stream,
            PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_STREAM)
        ),
        PDF_FIELD(
            "IndirectRef",
            &deserialized->indirect_ref,
            PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_INDIRECT_REF)
        )
    };

    DESERIALIZER_IMPL_HELPER();
}

TEST_FUNC(test_deserialize_objects) {
    Arena* arena = arena_new(1024);
    const char* objects[] = {
        "<< /Boolean true /Integer 42 /Real 42.5 /String (test) /Name /Hello /Array [1 2 3] /Dict << /A 1 /B 2 >> /Stream 2 0 R /IndirectRef 1 0 R >>",
        "<< /Length 8 >> stream\n01234567\nendstream\n"
    };

    char* buffer = pdf_construct_deserde_test_doc(
        objects,
        2,
        "<< /Size 3 /Root 404 0 R >>",
        arena
    );

    DESERIALIZER_TEST_HELPER();

    TestDeserializeObjectsStruct deserialized;
    TEST_PDF_REQUIRE(
        deserialize_test_objects_struct(&object, &deserialized, resolver)
    );

    TEST_ASSERT(deserialized.boolean);
    TEST_ASSERT_EQ(42, deserialized.integer);
    TEST_ASSERT_EQ(42.5, deserialized.real);
    TEST_ASSERT_EQ("test", pdf_string_as_cstr(deserialized.string, arena));
    TEST_ASSERT_EQ("Hello", deserialized.name);

    TEST_ASSERT(deserialized.array.elements);
    TEST_ASSERT_EQ((size_t)3, pdf_object_vec_len(deserialized.array.elements));
    for (size_t idx = 0; idx < 3; idx++) {
        PdfObject* element;
        TEST_ASSERT(
            pdf_object_vec_get(deserialized.array.elements, idx, &element)
        );
        TEST_ASSERT(element);
        TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_INTEGER, element->type);
        TEST_ASSERT_EQ((PdfInteger)idx + 1, element->data.integer);
    }

    TEST_ASSERT(deserialized.dict.entries);
    TEST_ASSERT_EQ(
        (size_t)2,
        pdf_dict_entry_vec_len(deserialized.dict.entries)
    );
    for (size_t idx = 0; idx < 2; idx++) {
        PdfDictEntry entry;
        TEST_ASSERT(
            pdf_dict_entry_vec_get(deserialized.dict.entries, idx, &entry)
        );

        TEST_ASSERT(entry.key);
        TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, entry.key->type);
        TEST_ASSERT_EQ(idx == 0 ? "A" : "B", entry.key->data.name);

        TEST_ASSERT(entry.value);
        TEST_ASSERT_EQ(
            (PdfObjectType)PDF_OBJECT_TYPE_INTEGER,
            entry.value->type
        );
        TEST_ASSERT_EQ((PdfInteger)idx + 1, entry.value->data.integer);
    }

    TEST_ASSERT(deserialized.stream.stream_dict);
    TEST_ASSERT(deserialized.stream.stream_bytes);
    TEST_ASSERT_EQ((size_t)8, deserialized.stream.decoded_stream_len);
    TEST_ASSERT(
        memcmp(
            "01234567",
            deserialized.stream.stream_bytes,
            deserialized.stream.decoded_stream_len
        )
        == 0
    );

    TEST_ASSERT_EQ((size_t)1, deserialized.indirect_ref.object_id);
    TEST_ASSERT_EQ((size_t)0, deserialized.indirect_ref.generation);

    return TEST_RESULT_PASS;
}

typedef struct {
    PdfName hello;
    PdfInteger world;
} TestDeserializeInnerStruct;

PdfError* deserialize_test_inner_struct(
    const PdfObject* object,
    TestDeserializeInnerStruct* deserialized,
    PdfResolver* resolver
) {
    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            "Hello",
            &deserialized->hello,
            PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(
            "World",
            &deserialized->world,
            PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_INTEGER)
        )
    };

    DESERIALIZER_IMPL_HELPER();
}

DESERDE_IMPL_TRAMPOLINE(
    deserialize_test_inner_struct_untyped,
    deserialize_test_inner_struct
)

DESERDE_DECL_RESOLVABLE(
    TestDeserializeInnerStructRef,
    TestDeserializeInnerStruct,
    test_deserialize_inner_struct_ref_init,
    test_deserialize_resolve_inner_struct
)

DESERDE_IMPL_RESOLVABLE(
    TestDeserializeInnerStructRef,
    TestDeserializeInnerStruct,
    test_deserialize_inner_struct_ref_init,
    test_deserialize_resolve_inner_struct,
    deserialize_test_inner_struct
)

typedef struct {
    TestDeserializeInnerStructRef reference;
} TestDeserializeRefStruct;

PdfError* deserialize_test_ref_struct(
    PdfObject* object,
    TestDeserializeRefStruct* deserialized,
    PdfResolver* resolver
) {
    PdfFieldDescriptor fields[] = {PDF_FIELD(
        "Reference",
        &deserialized->reference,
        PDF_DESERDE_RESOLVABLE(test_deserialize_inner_struct_ref_init)
    )};

    DESERIALIZER_IMPL_HELPER();
}

TEST_FUNC(test_deserialize_ref) {
    Arena* arena = arena_new(1024);
    const char* objects[] = {
        "<< /Reference 2 0 R >>",
        "<< /Hello /There /World 42 >>"
    };

    char* buffer = pdf_construct_deserde_test_doc(
        objects,
        2,
        "<< /Size 3 /Root 404 0 R >>",
        arena
    );

    DESERIALIZER_TEST_HELPER();

    TestDeserializeRefStruct deserialized;
    TEST_PDF_REQUIRE(
        deserialize_test_ref_struct(&object, &deserialized, resolver)
    );

    TestDeserializeInnerStruct resolved;
    TEST_PDF_REQUIRE(test_deserialize_resolve_inner_struct(
        deserialized.reference,
        resolver,
        &resolved
    ));

    TEST_ASSERT_EQ("There", resolved.hello);
    TEST_ASSERT_EQ(42, resolved.world);

    return TEST_RESULT_PASS;
}

typedef struct {
    TestDeserializeInnerStruct inner;
} TestDeserializeInlineStruct;

PdfError* deserialize_test_inline_struct(
    PdfObject* object,
    TestDeserializeInlineStruct* deserialized,
    PdfResolver* resolver
) {
    PdfFieldDescriptor fields[] = {PDF_FIELD(
        "Inner",
        &deserialized->inner,
        PDF_DESERDE_CUSTOM(deserialize_test_inner_struct_untyped)
    )};

    DESERIALIZER_IMPL_HELPER();
}

TEST_FUNC(test_deserialize_inline_struct) {
    Arena* arena = arena_new(1024);
    const char* objects[] = {
        "<< /Inner 2 0 R >>",
        "<< /Hello /There /World 42 >>"
    };

    char* buffer = pdf_construct_deserde_test_doc(
        objects,
        2,
        "<< /Size 3 /Root 404 0 R >>",
        arena
    );

    PdfResolver* resolver;
    TEST_PDF_REQUIRE(
        pdf_resolver_new(arena, (uint8_t*)buffer, strlen(buffer), &resolver)
    );

    PdfObject object;
    TEST_PDF_REQUIRE(pdf_resolve_ref(
        resolver,
        (PdfIndirectRef) {.object_id = 1, .generation = 0},
        &object
    ));

    TestDeserializeInlineStruct deserialized;
    TEST_PDF_REQUIRE(
        deserialize_test_inline_struct(&object, &deserialized, resolver)
    );

    TEST_ASSERT_EQ("There", deserialized.inner.hello);
    TEST_ASSERT_EQ(42, deserialized.inner.world);

    return TEST_RESULT_PASS;
}

DESERDE_DECL_OPTIONAL(
    TestDeserializeInnerOptional,
    TestDeserializeInnerStruct,
    test_deserialize_inner_struct_op_init
)

DESERDE_IMPL_OPTIONAL(
    TestDeserializeInnerOptional,
    test_deserialize_inner_struct_op_init
)

typedef struct {
    TestDeserializeInnerOptional inner;
} TestDeserializeInlineOptional;

PdfError* deserialize_test_inline_optional(
    PdfObject* object,
    TestDeserializeInlineOptional* deserialized,
    PdfResolver* resolver
) {
    PdfFieldDescriptor fields[] = {PDF_FIELD(
        "Inner",
        &deserialized->inner,
        PDF_DESERDE_OPTIONAL(
            test_deserialize_inner_struct_op_init,
            PDF_DESERDE_CUSTOM(deserialize_test_inner_struct_untyped)
        )
    )};

    DESERIALIZER_IMPL_HELPER();
}

TEST_FUNC(test_deserialize_inline_optional) {
    Arena* arena = arena_new(1024);
    const char* objects[] = {
        "<< /Inner 2 0 R >>",
        "<< /Hello /There /World 42 >>"
    };

    char* buffer = pdf_construct_deserde_test_doc(
        objects,
        2,
        "<< /Size 3 /Root 404 0 R >>",
        arena
    );

    DESERIALIZER_TEST_HELPER();

    TestDeserializeInlineOptional deserialized;
    TEST_PDF_REQUIRE(
        deserialize_test_inline_optional(&object, &deserialized, resolver)
    );

    TEST_ASSERT(deserialized.inner.has_value);
    TEST_ASSERT_EQ("There", deserialized.inner.value.hello);
    TEST_ASSERT_EQ(42, deserialized.inner.value.world);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_deserialize_inline_optional_none) {
    Arena* arena = arena_new(1024);
    const char* objects[] = {"<< >>"};

    char* buffer = pdf_construct_deserde_test_doc(
        objects,
        1,
        "<< /Size 2 /Root 404 0 R >>",
        arena
    );

    DESERIALIZER_TEST_HELPER();

    TestDeserializeInlineOptional deserialized = {.inner.has_value = true};
    TEST_PDF_REQUIRE(
        deserialize_test_inline_optional(&object, &deserialized, resolver)
    );

    TEST_ASSERT(!deserialized.inner.has_value);

    return TEST_RESULT_PASS;
}

#define DVEC_NAME DeserializeTestStringArray
#define DVEC_LOWERCASE_NAME deserialize_test_string_array
#define DVEC_TYPE PdfString
#include "arena/dvec_impl.h"

#define DVEC_NAME DeserializeTestIntegerArray
#define DVEC_LOWERCASE_NAME deserialize_test_integer_array
#define DVEC_TYPE PdfInteger
#include "arena/dvec_impl.h"

typedef struct {
    DeserializeTestStringArray* strings;
    DeserializeTestIntegerArray* integers;
} TestDeserializePrimitiveArrays;

PdfError* deserialize_test_primitive_arrays(
    PdfObject* object,
    PdfResolver* resolver,
    TestDeserializePrimitiveArrays* deserialized
) {
    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            "Strings",
            &deserialized->strings,
            PDF_DESERDE_ARRAY(
                deserialize_test_string_array_push_uninit,
                PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_STRING)
            )
        ),
        PDF_FIELD(
            "Integers",
            &deserialized->integers,
            PDF_DESERDE_ARRAY(
                deserialize_test_integer_array_push_uninit,
                PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_INTEGER)
            )
        )
    };

    DESERIALIZER_IMPL_HELPER();
}

TEST_FUNC(test_deserialize_primitive_arrays) {
    Arena* arena = arena_new(1024);
    const char* objects[] = {
        "<< /Strings [(Hello) (World)] /Integers [4 5 6] >>"
    };

    char* buffer = pdf_construct_deserde_test_doc(
        objects,
        1,
        "<< /Size 2 /Root 404 0 R >>",
        arena
    );

    DESERIALIZER_TEST_HELPER();

    TestDeserializePrimitiveArrays deserialized;
    TEST_PDF_REQUIRE(
        deserialize_test_primitive_arrays(&object, resolver, &deserialized)
    );

    TEST_ASSERT(deserialized.strings);
    TEST_ASSERT_EQ(
        (size_t)2,
        deserialize_test_string_array_len(deserialized.strings)
    );

    PdfString out_str;
    TEST_ASSERT(
        deserialize_test_string_array_get(deserialized.strings, 0, &out_str)
    );
    TEST_ASSERT_EQ("Hello", pdf_string_as_cstr(out_str, arena));
    TEST_ASSERT(
        deserialize_test_string_array_get(deserialized.strings, 1, &out_str)
    );
    TEST_ASSERT_EQ("World", pdf_string_as_cstr(out_str, arena));

    TEST_ASSERT(deserialized.integers);
    TEST_ASSERT_EQ(
        (size_t)3,
        deserialize_test_integer_array_len(deserialized.integers)
    );

    PdfInteger out_int;
    TEST_ASSERT(
        deserialize_test_integer_array_get(deserialized.integers, 0, &out_int)
    );
    TEST_ASSERT_EQ((PdfInteger)4, out_int);
    TEST_ASSERT(
        deserialize_test_integer_array_get(deserialized.integers, 1, &out_int)
    );
    TEST_ASSERT_EQ((PdfInteger)5, out_int);
    TEST_ASSERT(
        deserialize_test_integer_array_get(deserialized.integers, 2, &out_int)
    );
    TEST_ASSERT_EQ((PdfInteger)6, out_int);

    return TEST_RESULT_PASS;
}

#define DVEC_NAME DeserializeTestStructArray
#define DVEC_LOWERCASE_NAME deserialize_test_struct_array
#define DVEC_TYPE TestDeserializeInnerStruct
#include "arena/dvec_impl.h"

typedef struct {
    DeserializeTestStructArray* inners;
} TestDeserializeTestComplexArray;

PdfError* deserialize_test_complex_array(
    PdfObject* object,
    TestDeserializeTestComplexArray* deserialized,
    PdfResolver* resolver
) {
    PdfFieldDescriptor fields[] = {PDF_FIELD(
        "Inners",
        &deserialized->inners,
        PDF_DESERDE_ARRAY(
            deserialize_test_struct_array_push_uninit,
            PDF_DESERDE_CUSTOM(deserialize_test_inner_struct_untyped)
        )
    )};

    DESERIALIZER_IMPL_HELPER();
}

TEST_FUNC(test_deserialize_custom_array) {
    Arena* arena = arena_new(1024);
    const char* objects[] = {
        "<< /Inners [<< /Hello /There /World 42 >> << /Hello /Example /World 5 >>] >>"
    };

    char* buffer = pdf_construct_deserde_test_doc(
        objects,
        1,
        "<< /Size 2 /Root 404 0 R >>",
        arena
    );

    DESERIALIZER_TEST_HELPER();

    TestDeserializeTestComplexArray deserialized;
    TEST_PDF_REQUIRE(
        deserialize_test_complex_array(&object, &deserialized, resolver)
    );

    TEST_ASSERT(deserialized.inners);
    TEST_ASSERT_EQ(
        (size_t)2,
        deserialize_test_struct_array_len(deserialized.inners)
    );

    TestDeserializeInnerStruct element0;
    TEST_ASSERT(
        deserialize_test_struct_array_get(deserialized.inners, 0, &element0)
    );
    TEST_ASSERT_EQ("There", element0.hello);
    TEST_ASSERT_EQ(42, element0.world);

    TestDeserializeInnerStruct element1;
    TEST_ASSERT(
        deserialize_test_struct_array_get(deserialized.inners, 1, &element1)
    );
    TEST_ASSERT_EQ("Example", element1.hello);
    TEST_ASSERT_EQ(5, element1.world);

    return TEST_RESULT_PASS;
}

#endif // TEST
