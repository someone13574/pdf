#include "deserialize.h"

#include <string.h>

#include "arena.h"
#include "log.h"
#include "pdf_doc.h"
#include "pdf_object.h"
#include "pdf_result.h"
#include "vec.h"

PdfResult
resolve_object(PdfDocument* doc, PdfObject* object, PdfObject* resolved) {
    RELEASE_ASSERT(doc);
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(resolved);

    if (object->type == PDF_OBJECT_TYPE_INDIRECT_OBJECT) {
        LOG_TRACE_G("deserde", "Unwrapping indirect object");
        return resolve_object(
            doc,
            object->data.indirect_object.object,
            resolved
        );
    }

    if (object->type == PDF_OBJECT_TYPE_INDIRECT_REF) {
        LOG_TRACE_G("deserde", "Resolving indirect reference");

        PdfObject indirect_object;
        PDF_PROPAGATE(
            pdf_resolve(doc, object->data.indirect_ref, &indirect_object)
        );

        return resolve_object(doc, &indirect_object, resolved);
    }

    *resolved = *object;
    return PDF_OK;
}

PdfResult deserialize_object_field(
    void* field_ptr,
    PdfObject* object,
    PdfObjectFieldData field_data,
    PdfDocument* doc
);
PdfResult deserialize_ref_field(
    void* field_ptr,
    PdfObject* object,
    PdfRefFieldData field_data
);
PdfResult deserialize_custom_field(
    void* field_ptr,
    PdfObject* object,
    PdfDeserializerFn deserializer,
    PdfDocument* doc
);
PdfResult deserialize_array_field(
    void* field_ptr,
    PdfObject* object,
    PdfArrayFieldData field_data,
    PdfDocument* doc
);
PdfResult deserialize_as_array_field(
    void* field_ptr,
    PdfObject* object,
    PdfArrayFieldData field_data,
    PdfDocument* doc
);
PdfResult deserialize_optional_field(
    void* field_ptr,
    PdfObject* object,
    PdfOptionalFieldData field_data,
    PdfDocument* doc
);

PdfResult deserialize_field(
    void* field_ptr,
    PdfObject* object,
    PdfFieldInfo field_info,
    PdfDocument* doc
) {
    switch (field_info.kind) {
        case PDF_FIELD_KIND_OBJECT: {
            PDF_PROPAGATE(deserialize_object_field(
                field_ptr,
                object,
                field_info.data.object,
                doc
            ));
            break;
        }
        case PDF_FIELD_KIND_REF: {
            PDF_PROPAGATE(
                deserialize_ref_field(field_ptr, object, field_info.data.ref)
            );
            break;
        }
        case PDF_FIELD_KIND_CUSTOM: {
            PDF_PROPAGATE(deserialize_custom_field(
                field_ptr,
                object,
                field_info.data.custom,
                doc
            ));
            break;
        }
        case PDF_FIELD_KIND_ARRAY: {
            PDF_PROPAGATE(deserialize_array_field(
                field_ptr,
                object,
                field_info.data.array,
                doc
            ));
            break;
        }
        case PDF_FIELD_KIND_AS_ARRAY: {
            PDF_PROPAGATE(deserialize_as_array_field(
                field_ptr,
                object,
                field_info.data.array,
                doc
            ));
            break;
        }
        case PDF_FIELD_KIND_OPTIONAL: {
            PDF_PROPAGATE(deserialize_optional_field(
                field_ptr,
                object,
                field_info.data.optional,
                doc
            ));
            break;
        }
        case PDF_FIELD_KIND_IGNORED: {
            break;
        }
    }

    return PDF_OK;
}

PdfResult deserialize_object_field(
    void* field_ptr,
    PdfObject* object,
    PdfObjectFieldData field_data,
    PdfDocument* doc
) {
    RELEASE_ASSERT(field_ptr);
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(doc);

    PdfObject resolved_object;
    if (field_data.type == PDF_OBJECT_TYPE_INDIRECT_OBJECT
        || field_data.type == PDF_OBJECT_TYPE_INDIRECT_REF) {
        resolved_object = *object;
    } else {
        PDF_PROPAGATE(resolve_object(doc, object, &resolved_object));
    }

    LOG_TRACE_G(
        "deserde",
        "Deserializing object field with type `%d` and expected type `%d`",
        resolved_object.type,
        field_data.type
    );

    if (resolved_object.type != field_data.type) {
        LOG_ERROR_G(
            "deserde",
            "Incorrect type for object field. Expected `%d`, got `%d`",
            field_data.type,
            resolved_object.type
        );
        return PDF_ERR_INCORRECT_TYPE;
    }

    switch (resolved_object.type) {
        case PDF_OBJECT_TYPE_BOOLEAN: {
            *(PdfBoolean*)field_ptr = resolved_object.data.boolean;
            break;
        }
        case PDF_OBJECT_TYPE_INTEGER: {
            *(PdfInteger*)field_ptr = resolved_object.data.integer;
            break;
        }
        case PDF_OBJECT_TYPE_REAL: {
            *(PdfReal*)field_ptr = resolved_object.data.real;
            break;
        }
        case PDF_OBJECT_TYPE_STRING: {
            *(PdfString*)field_ptr = resolved_object.data.string;
            break;
        }
        case PDF_OBJECT_TYPE_NAME: {
            *(PdfName*)field_ptr = resolved_object.data.name;
            break;
        }
        case PDF_OBJECT_TYPE_ARRAY: {
            *(PdfArray*)field_ptr = resolved_object.data.array;
            break;
        }
        case PDF_OBJECT_TYPE_DICT: {
            *(PdfDict*)field_ptr = resolved_object.data.dict;
            break;
        }
        case PDF_OBJECT_TYPE_STREAM: {
            *(PdfStream*)field_ptr = resolved_object.data.stream;
            break;
        }
        case PDF_OBJECT_TYPE_INDIRECT_OBJECT: {
            *(PdfIndirectObject*)field_ptr =
                resolved_object.data.indirect_object;
            break;
        }
        case PDF_OBJECT_TYPE_INDIRECT_REF: {
            *(PdfIndirectRef*)field_ptr = resolved_object.data.indirect_ref;
            break;
        }
        case PDF_OBJECT_TYPE_NULL: {
            break;
        }
    }

    return PDF_OK;
}

PdfResult deserialize_ref_field(
    void* field_ptr,
    PdfObject* object,
    PdfRefFieldData field_data
) {
    RELEASE_ASSERT(field_ptr);
    RELEASE_ASSERT(object);

    if (object->type != PDF_OBJECT_TYPE_INDIRECT_REF) {
        LOG_ERROR_G(
            "deserde",
            "Incorrect type for ref field. Expected indirect ref, found %d",
            object->type
        );
        return PDF_ERR_INCORRECT_TYPE;
    }

    LOG_TRACE_G("deserde", "Deserializing ref field");

    void* object_ref_ptr = (char*)field_ptr + field_data.object_ref_offset;
    *(PdfIndirectRef*)object_ref_ptr = object->data.indirect_ref;

    return PDF_OK;
}

PdfResult deserialize_custom_field(
    void* field_ptr,
    PdfObject* object,
    PdfDeserializerFn deserializer,
    PdfDocument* doc
) {
    RELEASE_ASSERT(field_ptr);
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(deserializer);
    RELEASE_ASSERT(doc);

    return deserializer(object, doc, field_ptr);
}

PdfResult deserialize_array_element(
    PdfObject* element,
    PdfArrayFieldData field_data,
    PdfDocument* doc,
    void** deserialized
) {
    RELEASE_ASSERT(element);
    RELEASE_ASSERT(doc);
    RELEASE_ASSERT(deserialized);

    *deserialized = arena_alloc(pdf_doc_arena(doc), field_data.element_size);
    memset(*deserialized, 0, field_data.element_size);

    return deserialize_field(
        *deserialized,
        element,
        *field_data.element_info,
        doc
    );
}

PdfResult deserialize_array_field(
    void* field_ptr,
    PdfObject* object,
    PdfArrayFieldData field_data,
    PdfDocument* doc
) {
    RELEASE_ASSERT(field_ptr);
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(doc);

    PdfObject resolved_object;
    PDF_PROPAGATE(resolve_object(doc, object, &resolved_object));

    if (object->type != PDF_OBJECT_TYPE_ARRAY) {
        LOG_ERROR_G(
            "deserde",
            "Array field has incorrect type %d",
            object->type
        );
        return PDF_ERR_INCORRECT_TYPE;
    }

    LOG_TRACE_G(
        "deserde",
        "Deserializing array field with %zu elements",
        vec_len(object->data.array.elements)
    );

    Vec* deserialized_elements = vec_new(pdf_doc_arena(doc));
    for (size_t idx = 0; idx < vec_len(object->data.array.elements); idx++) {
        LOG_TRACE_G("deserde", "Deserializing array element %zu", idx);

        PdfObject* element = vec_get(object->data.array.elements, idx);

        void* deserialized;
        PDF_PROPAGATE(
            deserialize_array_element(element, field_data, doc, &deserialized)
        );

        vec_push(deserialized_elements, deserialized);
    }

    void* vec_ptr = (char*)field_ptr + field_data.vec_offset;
    *(Vec**)vec_ptr = deserialized_elements;

    return PDF_OK;
}

PdfResult deserialize_as_array_field(
    void* field_ptr,
    PdfObject* object,
    PdfArrayFieldData field_data,
    PdfDocument* doc
) {
    RELEASE_ASSERT(field_ptr);
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(doc);

    PdfObject resolved_object;
    PDF_PROPAGATE(resolve_object(doc, object, &resolved_object));

    if (object->type == PDF_OBJECT_TYPE_ARRAY) {
        return deserialize_array_field(field_ptr, object, field_data, doc);
    }

    LOG_TRACE_G("deserde", "Deserializing single element as array field");

    void* deserialized;
    PDF_PROPAGATE(
        deserialize_array_element(object, field_data, doc, &deserialized)
    );

    Vec* deserialized_elements = vec_new(pdf_doc_arena(doc));
    vec_push(deserialized_elements, deserialized);

    void* vec_ptr = (char*)field_ptr + field_data.vec_offset;
    LOG_INFO(
        "Vector offset: %zu, ptr=%p",
        field_data.vec_offset,
        (void*)vec_ptr
    );
    *(Vec**)vec_ptr = deserialized_elements;

    return PDF_OK;
}

PdfResult deserialize_optional_field(
    void* field_ptr,
    PdfObject* object,
    PdfOptionalFieldData field_data,
    PdfDocument* doc
) {
    RELEASE_ASSERT(field_ptr);
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(doc);

    void* discriminant_ptr = (char*)field_ptr + field_data.discriminant_offset;
    *(bool*)discriminant_ptr = true;

    LOG_TRACE_G("deserde", "Deserializing optional field");

    return deserialize_field(
        (char*)field_ptr + field_data.data_offset,
        object,
        *field_data.inner_info,
        doc
    );
}

PdfResult pdf_deserialize_object(
    void* target,
    PdfObject* object,
    PdfFieldDescriptor* fields,
    size_t num_fields,
    PdfDocument* doc
) {
    RELEASE_ASSERT(target);
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(fields);
    RELEASE_ASSERT(doc);

    // Check object type
    PDF_PROPAGATE(resolve_object(doc, object, object));

    if (object->type != PDF_OBJECT_TYPE_DICT) {
        return PDF_ERR_OBJECT_NOT_DICT;
    }

    LOG_DEBUG_G("deserde", "Deserializing dictionary object");

    // Reject unknown keys
    for (size_t entry_idx = 0; entry_idx < vec_len(object->data.dict.entries);
         entry_idx++) {
        bool found = false;
        PdfDictEntry* entry = vec_get(object->data.dict.entries, entry_idx);

        for (size_t field_idx = 0; field_idx < num_fields; field_idx++) {
            if (strcmp(entry->key->data.name, fields[field_idx].key) == 0) {
                if (found) {
                    LOG_ERROR_G(
                        "deserde",
                        "Duplicate dict key `%s`",
                        entry->key->data.name
                    );
                    return PDF_ERR_DUPLICATE_KEY;
                }

                found = true;
            }
        }

        if (!found) {
            LOG_ERROR_G(
                "deserde",
                "Dict key `%s` is not a known field",
                entry->key->data.name
            );
            return PDF_ERR_UNKNOWN_KEY;
        }
    }

    // Deserialize fields
    for (size_t field_idx = 0; field_idx < num_fields; field_idx++) {
        bool found = false;
        PdfFieldDescriptor* field = &fields[field_idx];
        LOG_TRACE_G(
            "deserde",
            "Field: `%s` (\x1b[4m%s:%lu\x1b[0m)",
            field->key,
            field->debug.file,
            field->debug.line
        );

        for (size_t entry_idx = 0;
             entry_idx < vec_len(object->data.dict.entries);
             entry_idx++) {
            PdfDictEntry* entry = vec_get(object->data.dict.entries, entry_idx);
            if (strcmp(entry->key->data.name, fields[field_idx].key) != 0) {
                continue;
            }

            PDF_PROPAGATE(deserialize_field(
                (char*)target + field->offset,
                entry->value,
                field->info,
                doc
            ));

            found = true;
            break;
        }

        if (!found && field->info.kind != PDF_FIELD_KIND_OPTIONAL) {
            LOG_DEBUG_G("deserde", "Missing key `%s`", field->key);
            return PDF_ERR_MISSING_DICT_KEY;
        }
    }

    LOG_TRACE_G("deserde", "Finished deserializing dictionary object");

    return PDF_OK;
}

#ifdef TEST

#include "deserialize_types.h"
#include "test.h"
#include "test_helpers.h"

#define DESERIALIZER_IMPL_HELPER()                                             \
    do {                                                                       \
        deserialized->raw_dict = object;                                       \
        PDF_PROPAGATE(pdf_deserialize_object(                                  \
            deserialized,                                                      \
            object,                                                            \
            fields,                                                            \
            sizeof(fields) / sizeof(PdfFieldDescriptor),                       \
            doc                                                                \
        ));                                                                    \
        return PDF_OK;                                                         \
    } while (0)

#define DESERIALIZER_TEST_HELPER()                                             \
    PdfDocument* doc;                                                          \
    TEST_ASSERT_EQ(                                                            \
        (PdfResult)PDF_OK,                                                     \
        pdf_document_new(arena, buffer, strlen(buffer), &doc)                  \
    );                                                                         \
    PdfObject object;                                                          \
    TEST_ASSERT_EQ(                                                            \
        (PdfResult)PDF_OK,                                                     \
        pdf_resolve(                                                           \
            doc,                                                               \
            (PdfIndirectRef) {.object_id = 1, .generation = 0},                \
            &object                                                            \
        )                                                                      \
    );

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
    PdfObject* raw_dict;
} TestDeserializeObjectsStruct;

PdfResult deserialize_test_objects_struct(
    PdfObject* object,
    PdfDocument* doc,
    TestDeserializeObjectsStruct* deserialized
) {
    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            TestDeserializeObjectsStruct,
            "Boolean",
            boolean,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_BOOLEAN)
        ),
        PDF_FIELD(
            TestDeserializeObjectsStruct,
            "Integer",
            integer,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_INTEGER)
        ),
        PDF_FIELD(
            TestDeserializeObjectsStruct,
            "Real",
            real,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_REAL)
        ),
        PDF_FIELD(
            TestDeserializeObjectsStruct,
            "String",
            string,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_STRING)
        ),
        PDF_FIELD(
            TestDeserializeObjectsStruct,
            "Name",
            name,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(
            TestDeserializeObjectsStruct,
            "Array",
            array,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_ARRAY)
        ),
        PDF_FIELD(
            TestDeserializeObjectsStruct,
            "Dict",
            dict,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_DICT)
        ),
        PDF_FIELD(
            TestDeserializeObjectsStruct,
            "Stream",
            stream,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_STREAM)
        ),
        PDF_FIELD(
            TestDeserializeObjectsStruct,
            "IndirectRef",
            indirect_ref,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_INDIRECT_REF)
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

    char* buffer =
        pdf_construct_deserde_test_doc(objects, 2, "<< /Size 3 >>", arena);
    LOG_DEBUG("Buffer:\n%s", buffer);

    DESERIALIZER_TEST_HELPER();

    TestDeserializeObjectsStruct deserialized;
    TEST_ASSERT_EQ(
        (PdfResult)PDF_OK,
        deserialize_test_objects_struct(&object, doc, &deserialized)
    );

    TEST_ASSERT_EQ(true, deserialized.boolean);
    TEST_ASSERT_EQ(42, deserialized.integer);
    TEST_ASSERT_EQ(42.5, deserialized.real);
    TEST_ASSERT_EQ("test", deserialized.string);
    TEST_ASSERT_EQ("Hello", deserialized.name);

    TEST_ASSERT(deserialized.array.elements);
    TEST_ASSERT_EQ((size_t)3, vec_len(deserialized.array.elements));
    for (size_t idx = 0; idx < 3; idx++) {
        PdfObject* element = vec_get(deserialized.array.elements, idx);
        TEST_ASSERT(element);
        TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_INTEGER, element->type);
        TEST_ASSERT_EQ((PdfInteger)idx + 1, element->data.integer);
    }

    TEST_ASSERT(deserialized.dict.entries);
    TEST_ASSERT_EQ((size_t)2, vec_len(deserialized.dict.entries));
    for (size_t idx = 0; idx < 2; idx++) {
        PdfDictEntry* entry = vec_get(deserialized.dict.entries, idx);
        TEST_ASSERT(entry);

        TEST_ASSERT(entry->key);
        TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, entry->key->type);
        TEST_ASSERT_EQ(idx == 0 ? "A" : "B", entry->key->data.name);

        TEST_ASSERT(entry->value);
        TEST_ASSERT_EQ(
            (PdfObjectType)PDF_OBJECT_TYPE_INTEGER,
            entry->value->type
        );
        TEST_ASSERT_EQ((PdfInteger)idx + 1, entry->value->data.integer);
    }

    TEST_ASSERT(deserialized.stream.stream_dict);
    TEST_ASSERT(deserialized.stream.stream_bytes);
    TEST_ASSERT_EQ("01234567", deserialized.stream.stream_bytes);

    TEST_ASSERT_EQ((size_t)1, deserialized.indirect_ref.object_id);
    TEST_ASSERT_EQ((size_t)0, deserialized.indirect_ref.generation);

    return TEST_RESULT_PASS;
}

typedef struct {
    PdfName hello;
    PdfInteger world;
    PdfObject* raw_dict;
} TestDeserializeInnerStruct;

PdfResult deserialize_test_inner_struct(
    PdfObject* object,
    PdfDocument* doc,
    TestDeserializeInnerStruct* deserialized
) {
    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            TestDeserializeInnerStruct,
            "Hello",
            hello,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(
            TestDeserializeInnerStruct,
            "World",
            world,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_INTEGER)
        )
    };

    DESERIALIZER_IMPL_HELPER();
}

PDF_UNTYPED_DESERIALIZER_WRAPPER(
    deserialize_test_inner_struct,
    deserialize_test_inner_struct_untyped
)

DESERIALIZABLE_STRUCT_REF(
    TestDeserializeInnerStruct,
    test_deserialize_inner_struct
)

PDF_DESERIALIZABLE_REF_IMPL(
    TestDeserializeInnerStruct,
    test_deserialize_inner_struct,
    deserialize_test_inner_struct
)

typedef struct {
    TestDeserializeInnerStructRef reference;
    PdfObject* raw_dict;
} TestDeserializeRefStruct;

PdfResult deserialize_test_ref_struct(
    PdfObject* object,
    PdfDocument* doc,
    TestDeserializeRefStruct* deserialized
) {
    PdfFieldDescriptor fields[] = {PDF_FIELD(
        TestDeserializeRefStruct,
        "Reference",
        reference,
        PDF_REF_FIELD(TestDeserializeInnerStructRef)
    )};

    DESERIALIZER_IMPL_HELPER();
}

TEST_FUNC(test_deserialize_ref) {
    Arena* arena = arena_new(1024);
    const char* objects[] = {
        "<< /Reference 2 0 R >>",
        "<< /Hello /There /World 42 >>"
    };

    char* buffer =
        pdf_construct_deserde_test_doc(objects, 2, "<< /Size 3 >>", arena);
    LOG_DEBUG("Buffer:\n%s", buffer);

    DESERIALIZER_TEST_HELPER();

    TestDeserializeRefStruct deserialized;
    TEST_ASSERT_EQ(
        (PdfResult)PDF_OK,
        deserialize_test_ref_struct(&object, doc, &deserialized)
    );

    TestDeserializeInnerStruct resolved;
    TEST_ASSERT_EQ(
        (PdfResult)PDF_OK,
        pdf_resolve_test_deserialize_inner_struct(
            &deserialized.reference,
            doc,
            &resolved
        )
    );

    TEST_ASSERT_EQ("There", resolved.hello);
    TEST_ASSERT_EQ(42, resolved.world);

    return TEST_RESULT_PASS;
}

typedef struct {
    TestDeserializeInnerStruct inner;
    PdfObject* raw_dict;
} TestDeserializeInlineStruct;

PdfResult deserialize_test_inline_struct(
    PdfObject* object,
    PdfDocument* doc,
    TestDeserializeInlineStruct* deserialized
) {
    PdfFieldDescriptor fields[] = {PDF_FIELD(
        TestDeserializeInlineStruct,
        "Inner",
        inner,
        PDF_CUSTOM_FIELD(deserialize_test_inner_struct_untyped)
    )};

    DESERIALIZER_IMPL_HELPER();
}

TEST_FUNC(test_deserialize_inline_struct) {
    Arena* arena = arena_new(1024);
    const char* objects[] = {
        "<< /Inner 2 0 R >>",
        "<< /Hello /There /World 42 >>"
    };

    char* buffer =
        pdf_construct_deserde_test_doc(objects, 2, "<< /Size 3 >>", arena);
    LOG_DEBUG("Buffer:\n%s", buffer);

    PdfDocument* doc;
    TEST_ASSERT_EQ(
        (PdfResult)PDF_OK,
        pdf_document_new(arena, buffer, strlen(buffer), &doc)
    );

    PdfObject object;
    TEST_ASSERT_EQ(
        (PdfResult)PDF_OK,
        pdf_resolve(
            doc,
            (PdfIndirectRef) {.object_id = 1, .generation = 0},
            &object
        )
    );

    TestDeserializeInlineStruct deserialized;
    TEST_ASSERT_EQ(
        (PdfResult)PDF_OK,
        deserialize_test_inline_struct(&object, doc, &deserialized)
    );

    TEST_ASSERT_EQ("There", deserialized.inner.hello);
    TEST_ASSERT_EQ(42, deserialized.inner.world);

    return TEST_RESULT_PASS;
}

DESERIALIZABLE_OPTIONAL_TYPE(
    TestDeserializeInnerOptional,
    TestDeserializeInnerStruct
)

typedef struct {
    TestDeserializeInnerOptional inner;
    PdfObject* raw_dict;
} TestDeserializeInlineOptional;

PdfResult deserialize_test_inline_optional(
    PdfObject* object,
    PdfDocument* doc,
    TestDeserializeInlineOptional* deserialized
) {
    PdfFieldDescriptor fields[] = {PDF_FIELD(
        TestDeserializeInlineOptional,
        "Inner",
        inner,
        PDF_OPTIONAL_FIELD(
            TestDeserializeInnerOptional,
            PDF_CUSTOM_FIELD(deserialize_test_inner_struct_untyped)
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

    char* buffer =
        pdf_construct_deserde_test_doc(objects, 2, "<< /Size 3 >>", arena);
    LOG_DEBUG("Buffer:\n%s", buffer);

    DESERIALIZER_TEST_HELPER();

    TestDeserializeInlineOptional deserialized;
    TEST_ASSERT_EQ(
        (PdfResult)PDF_OK,
        deserialize_test_inline_optional(&object, doc, &deserialized)
    );

    TEST_ASSERT(deserialized.inner.discriminant);
    TEST_ASSERT_EQ("There", deserialized.inner.value.hello);
    TEST_ASSERT_EQ(42, deserialized.inner.value.world);

    return TEST_RESULT_PASS;
}

DESERIALIZABLE_ARRAY_TYPE(DeserializeTestStringArray)
DESERIALIZABLE_ARRAY_TYPE(DeserializeTestIntegerArray)

typedef struct {
    DeserializeTestStringArray strings;
    DeserializeTestIntegerArray integers;
    PdfObject* raw_dict;
} TestDeserializePrimitiveArrays;

PdfResult deserialize_test_primitive_arrays(
    PdfObject* object,
    PdfDocument* doc,
    TestDeserializePrimitiveArrays* deserialized
) {
    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            TestDeserializePrimitiveArrays,
            "Strings",
            strings,
            PDF_ARRAY_FIELD(
                DeserializeTestStringArray,
                PdfString,
                PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_STRING)
            )
        ),
        PDF_FIELD(
            TestDeserializePrimitiveArrays,
            "Integers",
            integers,
            PDF_ARRAY_FIELD(
                DeserializeTestIntegerArray,
                PdfInteger,
                PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_INTEGER)
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

    char* buffer =
        pdf_construct_deserde_test_doc(objects, 1, "<< /Size 2 >>", arena);
    LOG_DEBUG("Buffer:\n%s", buffer);

    DESERIALIZER_TEST_HELPER();

    TestDeserializePrimitiveArrays deserialized;
    TEST_ASSERT_EQ(
        (PdfResult)PDF_OK,
        deserialize_test_primitive_arrays(&object, doc, &deserialized)
    );

    TEST_ASSERT(deserialized.strings.elements);
    TEST_ASSERT_EQ((size_t)2, vec_len(deserialized.strings.elements));
    TEST_ASSERT_EQ(
        "Hello",
        *(PdfString*)vec_get(deserialized.strings.elements, 0)
    );
    TEST_ASSERT_EQ(
        "World",
        *(PdfString*)vec_get(deserialized.strings.elements, 1)
    );

    TEST_ASSERT(deserialized.integers.elements);
    TEST_ASSERT_EQ((size_t)3, vec_len(deserialized.integers.elements));
    TEST_ASSERT_EQ(
        (PdfInteger)4,
        *(PdfInteger*)vec_get(deserialized.integers.elements, 0)
    );
    TEST_ASSERT_EQ(
        (PdfInteger)5,
        *(PdfInteger*)vec_get(deserialized.integers.elements, 1)
    );
    TEST_ASSERT_EQ(
        (PdfInteger)6,
        *(PdfInteger*)vec_get(deserialized.integers.elements, 2)
    );

    return TEST_RESULT_PASS;
}

DESERIALIZABLE_ARRAY_TYPE(DeserializeTestStructArray)

typedef struct {
    DeserializeTestStructArray inners;
    PdfObject* raw_dict;
} TestDeserializeTestComplexArray;

PdfResult deserialize_test_complex_array(
    PdfObject* object,
    PdfDocument* doc,
    TestDeserializeTestComplexArray* deserialized
) {
    PdfFieldDescriptor fields[] = {PDF_FIELD(
        TestDeserializeTestComplexArray,
        "Inners",
        inners,
        PDF_ARRAY_FIELD(
            DeserializeTestStructArray,
            TestDeserializeInnerStruct,
            PDF_CUSTOM_FIELD(deserialize_test_inner_struct_untyped)
        )
    )};

    DESERIALIZER_IMPL_HELPER();
}

TEST_FUNC(test_deserialize_custom_array) {
    Arena* arena = arena_new(1024);
    const char* objects[] = {
        "<< /Inners [<< /Hello /There /World 42 >> << /Hello /Example /World 5 >>] >>"
    };

    char* buffer =
        pdf_construct_deserde_test_doc(objects, 1, "<< /Size 2 >>", arena);
    LOG_DEBUG("Buffer:\n%s", buffer);

    DESERIALIZER_TEST_HELPER();

    TestDeserializeTestComplexArray deserialized;
    TEST_ASSERT_EQ(
        (PdfResult)PDF_OK,
        deserialize_test_complex_array(&object, doc, &deserialized)
    );

    TEST_ASSERT(deserialized.inners.elements);
    TEST_ASSERT_EQ((size_t)2, vec_len(deserialized.inners.elements));

    TestDeserializeInnerStruct* element0 =
        vec_get(deserialized.inners.elements, 0);
    TEST_ASSERT(element0);
    TEST_ASSERT_EQ("There", element0->hello);
    TEST_ASSERT_EQ(42, element0->world);

    TestDeserializeInnerStruct* element1 =
        vec_get(deserialized.inners.elements, 1);
    TEST_ASSERT(element1);
    TEST_ASSERT_EQ("Example", element1->hello);
    TEST_ASSERT_EQ(5, element1->world);

    return TEST_RESULT_PASS;
}

#endif // TEST
