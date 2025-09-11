#include "deserialize.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "arena/arena.h"
#include "logger/log.h"
#include "pdf/deserde_types.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"

PdfError* pdf_resolve_object(
    PdfOptionalResolver resolver,
    const PdfObject* object,
    PdfObject* resolved
) {
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(resolved);

    if (object->type == PDF_OBJECT_TYPE_INDIRECT_OBJECT
        && resolver.unwrap_indirect_objs) {
        LOG_DIAG(TRACE, DESERDE, "Unwrapping indirect object");
        return pdf_resolve_object(
            resolver,
            object->data.indirect_object.object,
            resolved
        );
    }

    if (object->type == PDF_OBJECT_TYPE_INDIRECT_REF && resolver.present) {
        LOG_DIAG(TRACE, DESERDE, "Resolving indirect reference");

        PdfObject indirect_object;
        PDF_PROPAGATE(pdf_resolve_ref(
            resolver.resolver,
            object->data.indirect_ref,
            &indirect_object
        ));

        return pdf_resolve_object(resolver, &indirect_object, resolved);
    }

    LOG_DIAG(TRACE, DESERDE, "Resolved type is %d", object->type);

    *resolved = *object;
    return NULL;
}

PdfError* deserialize_field(
    void* field_ptr,
    const PdfObject* object,
    PdfFieldInfo field_info,
    Arena* arena,
    PdfOptionalResolver resolver
) {
    RELEASE_ASSERT(field_ptr);
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));

    PdfObject resolved_object;

    bool is_ref_field = field_info.kind == PDF_FIELD_KIND_REF;
    bool is_indirect_obj_field =
        field_info.kind == PDF_FIELD_KIND_OBJECT
        && (field_info.data.object.type == PDF_OBJECT_TYPE_INDIRECT_REF
            || field_info.data.object.type == PDF_OBJECT_TYPE_INDIRECT_OBJECT);
    if (is_ref_field || is_indirect_obj_field) {
        resolved_object = *object;
    } else {
        PDF_PROPAGATE(pdf_resolve_object(resolver, object, &resolved_object));
    }

    switch (field_info.kind) {
        case PDF_FIELD_KIND_OBJECT: {
            PDF_PROPAGATE(pdf_deserialize_object_field(
                field_ptr,
                &resolved_object,
                field_info.data.object
            ));
            break;
        }
        case PDF_FIELD_KIND_REF: {
            PDF_PROPAGATE(pdf_deserialize_ref_field(
                field_ptr,
                &resolved_object,
                field_info.data.ref
            ));
            break;
        }
        case PDF_FIELD_KIND_CUSTOM: {
            PDF_PROPAGATE(pdf_deserialize_custom_field(
                field_ptr,
                &resolved_object,
                arena,
                field_info.data.custom,
                resolver
            ));
            break;
        }
        case PDF_FIELD_KIND_ARRAY: {
            PDF_PROPAGATE(pdf_deserialize_array_field(
                field_ptr,
                &resolved_object,
                field_info.data.array,
                arena,
                resolver
            ));
            break;
        }
        case PDF_FIELD_KIND_AS_ARRAY: {
            PDF_PROPAGATE(pdf_deserialize_as_array_field(
                field_ptr,
                &resolved_object,
                field_info.data.array,
                arena,
                resolver
            ));
            break;
        }
        case PDF_FIELD_KIND_OPTIONAL: {
            PDF_PROPAGATE(pdf_deserialize_optional_field(
                field_ptr,
                &resolved_object,
                field_info.data.optional,
                arena,
                resolver
            ));
            break;
        }
        case PDF_FIELD_KIND_IGNORED: {
            break;
        }
    }

    return NULL;
}

PdfError* pdf_deserialize_object_field(
    void* field_ptr,
    const PdfObject* object,
    PdfObjectFieldData field_data
) {
    RELEASE_ASSERT(field_ptr);
    RELEASE_ASSERT(object);

    LOG_DIAG(TRACE, DESERDE, "Deserializing object field");

    if (object->type != field_data.type) {
        return PDF_ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "Incorrect type for object field. Expected `%d`, got `%d`",
            field_data.type,
            object->type
        );
    }

    switch (object->type) {
        case PDF_OBJECT_TYPE_BOOLEAN: {
            *(PdfBoolean*)field_ptr = object->data.boolean;
            break;
        }
        case PDF_OBJECT_TYPE_INTEGER: {
            *(PdfInteger*)field_ptr = object->data.integer;
            break;
        }
        case PDF_OBJECT_TYPE_REAL: {
            *(PdfReal*)field_ptr = object->data.real;
            break;
        }
        case PDF_OBJECT_TYPE_STRING: {
            *(PdfString*)field_ptr = object->data.string;
            break;
        }
        case PDF_OBJECT_TYPE_NAME: {
            *(PdfName*)field_ptr = object->data.name;
            break;
        }
        case PDF_OBJECT_TYPE_ARRAY: {
            *(PdfArray*)field_ptr = object->data.array;
            break;
        }
        case PDF_OBJECT_TYPE_DICT: {
            *(PdfDict*)field_ptr = object->data.dict;
            break;
        }
        case PDF_OBJECT_TYPE_STREAM: {
            *(PdfStream*)field_ptr = object->data.stream;
            break;
        }
        case PDF_OBJECT_TYPE_INDIRECT_OBJECT: {
            *(PdfIndirectObject*)field_ptr = object->data.indirect_object;
            break;
        }
        case PDF_OBJECT_TYPE_INDIRECT_REF: {
            *(PdfIndirectRef*)field_ptr = object->data.indirect_ref;
            break;
        }
        case PDF_OBJECT_TYPE_NULL: {
            break;
        }
    }

    return NULL;
}

PdfError* pdf_deserialize_ref_field(
    void* field_ptr,
    const PdfObject* object,
    PdfRefFieldData field_data
) {
    RELEASE_ASSERT(field_ptr);
    RELEASE_ASSERT(object);

    if (object->type != PDF_OBJECT_TYPE_INDIRECT_REF) {
        return PDF_ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "Incorrect type for ref field. Expected indirect ref, found %d",
            object->type
        );
    }

    LOG_DIAG(TRACE, DESERDE, "Deserializing ref field");

    void* object_ref_ptr = (char*)field_ptr + field_data.object_ref_offset;
    *(PdfIndirectRef*)object_ref_ptr = object->data.indirect_ref;

    return NULL;
}

PdfError* pdf_deserialize_custom_field(
    void* field_ptr,
    const PdfObject* object,
    Arena* arena,
    PdfDeserializerFn deserializer,
    PdfOptionalResolver resolver
) {
    RELEASE_ASSERT(field_ptr);
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(deserializer);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));

    return deserializer(object, arena, resolver, field_ptr);
}

PdfError* deserialize_array_element(
    const PdfObject* element,
    PdfArrayFieldData field_data,
    Arena* arena,
    PdfOptionalResolver resolver,
    void** deserialized
) {
    RELEASE_ASSERT(element);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));
    RELEASE_ASSERT(deserialized);

    *deserialized = arena_alloc(arena, field_data.element_size);
    memset(*deserialized, 0, field_data.element_size);

    return deserialize_field(
        *deserialized,
        element,
        *field_data.element_info,
        arena,
        resolver
    );
}

PdfError* pdf_deserialize_array_field(
    void* field_ptr,
    const PdfObject* object,
    PdfArrayFieldData field_data,
    Arena* arena,
    PdfOptionalResolver resolver
) {
    RELEASE_ASSERT(field_ptr);
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));

    if (object->type != PDF_OBJECT_TYPE_ARRAY) {
        return PDF_ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "Array field has incorrect type %d",
            object->type
        );
    }

    LOG_DIAG(
        TRACE,
        DESERDE,
        "Deserializing array field with %zu elements",
        pdf_object_vec_len(object->data.array.elements)
    );

    PdfVoidVec* deserialized_elements = pdf_void_vec_new(arena);
    for (size_t idx = 0; idx < pdf_object_vec_len(object->data.array.elements);
         idx++) {
        LOG_DIAG(TRACE, DESERDE, "Deserializing array element %zu", idx);

        PdfObject* element = NULL;
        RELEASE_ASSERT(
            pdf_object_vec_get(object->data.array.elements, idx, &element)
        );

        void* deserialized;
        PDF_PROPAGATE(deserialize_array_element(
            element,
            field_data,
            arena,
            resolver,
            &deserialized
        ));

        pdf_void_vec_push(deserialized_elements, deserialized);
    }

    void* vec_ptr = (char*)field_ptr + field_data.vec_offset;
    *(PdfVoidVec**)vec_ptr = deserialized_elements;

    return NULL;
}

PdfError* pdf_deserialize_as_array_field(
    void* field_ptr,
    const PdfObject* object,
    PdfArrayFieldData field_data,
    Arena* arena,
    PdfOptionalResolver resolver
) {
    RELEASE_ASSERT(field_ptr);
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));

    if (object->type == PDF_OBJECT_TYPE_ARRAY) {
        return pdf_deserialize_array_field(
            field_ptr,
            object,
            field_data,
            arena,
            resolver
        );
    }

    LOG_DIAG(TRACE, DESERDE, "Deserializing single element as array field");

    void* deserialized;
    PDF_PROPAGATE(deserialize_array_element(
        object,
        field_data,
        arena,
        resolver,
        &deserialized
    ));

    PdfVoidVec* deserialized_elements = pdf_void_vec_new(arena);
    pdf_void_vec_push(deserialized_elements, deserialized);

    void* vec_ptr = (char*)field_ptr + field_data.vec_offset;
    *(PdfVoidVec**)vec_ptr = deserialized_elements;

    return NULL;
}

PdfError* pdf_deserialize_optional_field(
    void* field_ptr,
    const PdfObject* object,
    PdfOptionalFieldData field_data,
    Arena* arena,
    PdfOptionalResolver resolver
) {
    RELEASE_ASSERT(field_ptr);
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));

    void* discriminant_ptr = (char*)field_ptr + field_data.discriminant_offset;
    *(bool*)discriminant_ptr = true;

    LOG_DIAG(TRACE, DESERDE, "Deserializing optional field");

    return deserialize_field(
        (char*)field_ptr + field_data.data_offset,
        object,
        *field_data.inner_info,
        arena,
        resolver
    );
}

static void
pdf_optional_field_none(void* field_ptr, PdfOptionalFieldData field_data) {
    RELEASE_ASSERT(field_ptr);

    void* discriminant_ptr = (char*)field_ptr + field_data.discriminant_offset;
    *(bool*)discriminant_ptr = false;
}

PdfError* pdf_deserialize_object(
    void* target,
    const PdfObject* object,
    const PdfFieldDescriptor* fields,
    size_t num_fields,
    Arena* arena,
    PdfOptionalResolver resolver,
    bool allow_unknown_fields
) {
    RELEASE_ASSERT(target);
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(fields);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));

    // Resolve object
    PdfObject resolved_object;
    PDF_PROPAGATE(pdf_resolve_object(resolver, object, &resolved_object));

    // Check object type
    if (resolved_object.type != PDF_OBJECT_TYPE_DICT) {
        return PDF_ERROR(PDF_ERR_OBJECT_NOT_DICT);
    }

    LOG_DIAG(INFO, DESERDE, "Deserializing dictionary object");

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
            "Field: `%s` (\x1b[4m%s:%lu\x1b[0m) (kind=%d)",
            field->key,
            field->debug.file,
            field->debug.line,
            field->info.kind
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

            PDF_PROPAGATE(deserialize_field(
                (char*)target + field->offset,
                entry.value,
                field->info,
                arena,
                resolver
            ));

            found = true;
            break;
        }

        if (!found) {
            if (field->info.kind == PDF_FIELD_KIND_OPTIONAL) {
                pdf_optional_field_none(
                    (char*)target + field->offset,
                    field->info.data.optional
                );
            } else {
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
    void* target,
    const PdfOperandDescriptor* descriptors,
    size_t num_descriptors,
    const PdfObjectVec* operands,
    Arena* arena
) {
    RELEASE_ASSERT(target);
    RELEASE_ASSERT(descriptors);
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(arena);

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
        void* field_ptr = (char*)target + descriptor.offset;

        switch (descriptor.info.kind) {
            case PDF_FIELD_KIND_OBJECT: {
                PDF_PROPAGATE(pdf_deserialize_object_field(
                    field_ptr,
                    operand,
                    descriptor.info.data.object
                ));
                break;
            }
            case PDF_FIELD_KIND_CUSTOM: {
                PDF_PROPAGATE(pdf_deserialize_custom_field(
                    field_ptr,
                    operand,
                    arena,
                    descriptor.info.data.custom,
                    pdf_op_resolver_none(false)
                ));
                break;
            }
            case PDF_FIELD_KIND_ARRAY: {
                PDF_PROPAGATE(pdf_deserialize_array_field(
                    field_ptr,
                    operand,
                    descriptor.info.data.array,
                    arena,
                    pdf_op_resolver_none(false)
                ));
                break;
            }
            case PDF_FIELD_KIND_AS_ARRAY: {
                PDF_PROPAGATE(pdf_deserialize_array_field(
                    field_ptr,
                    operand,
                    descriptor.info.data.array,
                    arena,
                    pdf_op_resolver_none(false)
                ));
                break;
            }
            case PDF_FIELD_KIND_IGNORED: {
                break;
            }
            default: {
                return PDF_ERROR(PDF_ERR_INVALID_OPERAND_DESCRIPTOR);
            }
        }
    }

    return NULL;
}

#ifdef TEST

#include "object.h"
#include "pdf/deserde_types.h"
#include "test/test.h"
#include "test_helpers.h"

#define DESERIALIZER_IMPL_HELPER()                                             \
    do {                                                                       \
        deserialized->raw_dict = object;                                       \
        PDF_PROPAGATE(pdf_deserialize_object(                                  \
            deserialized,                                                      \
            object,                                                            \
            fields,                                                            \
            sizeof(fields) / sizeof(PdfFieldDescriptor),                       \
            arena,                                                             \
            resolver,                                                          \
            false                                                              \
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
    const PdfObject* raw_dict;
} TestDeserializeObjectsStruct;

PdfError* deserialize_test_objects_struct(
    PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
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

    DESERIALIZER_TEST_HELPER();

    TestDeserializeObjectsStruct deserialized;
    TEST_PDF_REQUIRE(deserialize_test_objects_struct(
        &object,
        arena,
        pdf_op_resolver_some(resolver),
        &deserialized
    ));

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
    const PdfObject* raw_dict;
} TestDeserializeInnerStruct;

PdfError* deserialize_test_inner_struct(
    const PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
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
    const PdfObject* raw_dict;
} TestDeserializeRefStruct;

PdfError* deserialize_test_ref_struct(
    PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
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

    DESERIALIZER_TEST_HELPER();

    TestDeserializeRefStruct deserialized;
    TEST_PDF_REQUIRE(deserialize_test_ref_struct(
        &object,
        arena,
        pdf_op_resolver_some(resolver),
        &deserialized
    ));

    TestDeserializeInnerStruct resolved;
    TEST_PDF_REQUIRE(pdf_resolve_test_deserialize_inner_struct(
        &deserialized.reference,
        resolver,
        &resolved
    ));

    TEST_ASSERT_EQ("There", resolved.hello);
    TEST_ASSERT_EQ(42, resolved.world);

    return TEST_RESULT_PASS;
}

typedef struct {
    TestDeserializeInnerStruct inner;
    const PdfObject* raw_dict;
} TestDeserializeInlineStruct;

PdfError* deserialize_test_inline_struct(
    PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
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
    TEST_PDF_REQUIRE(deserialize_test_inline_struct(
        &object,
        arena,
        pdf_op_resolver_some(resolver),
        &deserialized
    ));

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
    const PdfObject* raw_dict;
} TestDeserializeInlineOptional;

PdfError* deserialize_test_inline_optional(
    PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
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

    DESERIALIZER_TEST_HELPER();

    TestDeserializeInlineOptional deserialized;
    TEST_PDF_REQUIRE(deserialize_test_inline_optional(
        &object,
        arena,
        pdf_op_resolver_some(resolver),
        &deserialized
    ));

    TEST_ASSERT(deserialized.inner.discriminant);
    TEST_ASSERT_EQ("There", deserialized.inner.value.hello);
    TEST_ASSERT_EQ(42, deserialized.inner.value.world);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_deserialize_inline_optional_none) {
    Arena* arena = arena_new(1024);
    const char* objects[] = {"<< >>"};

    char* buffer =
        pdf_construct_deserde_test_doc(objects, 1, "<< /Size 2 >>", arena);

    DESERIALIZER_TEST_HELPER();

    TestDeserializeInlineOptional deserialized = {.inner.discriminant = true};
    TEST_PDF_REQUIRE(deserialize_test_inline_optional(
        &object,
        arena,
        pdf_op_resolver_some(resolver),
        &deserialized
    ));

    TEST_ASSERT(!deserialized.inner.discriminant);

    return TEST_RESULT_PASS;
}

DESERIALIZABLE_ARRAY_TYPE(DeserializeTestStringArray)
DESERIALIZABLE_ARRAY_TYPE(DeserializeTestIntegerArray)

typedef struct {
    DeserializeTestStringArray strings;
    DeserializeTestIntegerArray integers;
    const PdfObject* raw_dict;
} TestDeserializePrimitiveArrays;

PdfError* deserialize_test_primitive_arrays(
    PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
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

    DESERIALIZER_TEST_HELPER();

    TestDeserializePrimitiveArrays deserialized;
    TEST_PDF_REQUIRE(deserialize_test_primitive_arrays(
        &object,
        arena,
        pdf_op_resolver_some(resolver),
        &deserialized
    ));

    TEST_ASSERT(deserialized.strings.elements);
    TEST_ASSERT_EQ((size_t)2, pdf_void_vec_len(deserialized.strings.elements));

    void* out;
    TEST_ASSERT(pdf_void_vec_get(deserialized.strings.elements, 0, &out));
    TEST_ASSERT_EQ("Hello", pdf_string_as_cstr(*(PdfString*)out, arena));
    TEST_ASSERT(pdf_void_vec_get(deserialized.strings.elements, 1, &out));
    TEST_ASSERT_EQ("World", pdf_string_as_cstr(*(PdfString*)out, arena));

    TEST_ASSERT(deserialized.integers.elements);
    TEST_ASSERT_EQ((size_t)3, pdf_void_vec_len(deserialized.integers.elements));

    TEST_ASSERT(pdf_void_vec_get(deserialized.integers.elements, 0, &out));
    TEST_ASSERT_EQ((PdfInteger)4, *(PdfInteger*)out);
    TEST_ASSERT(pdf_void_vec_get(deserialized.integers.elements, 1, &out));
    TEST_ASSERT_EQ((PdfInteger)5, *(PdfInteger*)out);
    TEST_ASSERT(pdf_void_vec_get(deserialized.integers.elements, 2, &out));
    TEST_ASSERT_EQ((PdfInteger)6, *(PdfInteger*)out);

    return TEST_RESULT_PASS;
}

DESERIALIZABLE_ARRAY_TYPE(DeserializeTestStructArray)

typedef struct {
    DeserializeTestStructArray inners;
    const PdfObject* raw_dict;
} TestDeserializeTestComplexArray;

PdfError* deserialize_test_complex_array(
    PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
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

    DESERIALIZER_TEST_HELPER();

    TestDeserializeTestComplexArray deserialized;
    TEST_PDF_REQUIRE(deserialize_test_complex_array(
        &object,
        arena,
        pdf_op_resolver_some(resolver),
        &deserialized
    ));

    TEST_ASSERT(deserialized.inners.elements);
    TEST_ASSERT_EQ((size_t)2, pdf_void_vec_len(deserialized.inners.elements));

    void* element0;
    TEST_ASSERT(pdf_void_vec_get(deserialized.inners.elements, 0, &element0));
    TEST_ASSERT(element0);
    TEST_ASSERT_EQ("There", ((TestDeserializeInnerStruct*)element0)->hello);
    TEST_ASSERT_EQ(42, ((TestDeserializeInnerStruct*)element0)->world);

    void* element1;
    TEST_ASSERT(pdf_void_vec_get(deserialized.inners.elements, 1, &element1));
    TEST_ASSERT(element1);
    TEST_ASSERT_EQ("Example", ((TestDeserializeInnerStruct*)element1)->hello);
    TEST_ASSERT_EQ(5, ((TestDeserializeInnerStruct*)element1)->world);

    return TEST_RESULT_PASS;
}

#endif // TEST
