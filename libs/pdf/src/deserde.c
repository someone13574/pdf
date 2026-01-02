#include "pdf/deserde.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "err/error.h"
#include "logger/log.h"
#include "pdf/object.h"
#include "pdf/resolver.h"

Error* pdf_deserde_fields(
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
    TRY(pdf_resolve_object(resolver, object, &resolved_object, true));

    // Check object type
    if (resolved_object.type != PDF_OBJECT_TYPE_DICT) {
        return ERROR(
            PDF_ERR_OBJECT_NOT_DICT,
            "Object type is not a dictionary. Type is %d",
            resolved_object.type
        );
    }

    LOG_DIAG(
        INFO,
        DESER,
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
                if (strcmp(entry.key, fields[field_idx].key) == 0) {
                    if (found) {
                        return ERROR(
                            PDF_ERR_DUPLICATE_KEY,
                            "Duplicate dict key `%s`",
                            entry.key
                        );
                    }

                    found = true;
                }
            }

            if (!found) {
                return ERROR(
                    PDF_ERR_UNKNOWN_KEY,
                    "Dict key `%s` is not a known field",
                    entry.key
                );
            }
        }
    }

    // Deserialize fields
    for (size_t field_idx = 0; field_idx < num_fields; field_idx++) {
        bool found = false;
        const PdfFieldDescriptor* field = &fields[field_idx];
        LOG_DIAG(DEBUG, DESER, "Field: `%s`", field->key);

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
            if (strcmp(entry.key, fields[field_idx].key) != 0) {
                continue;
            }

            TRY(field->deserializer(&entry.value, field->target_ptr, resolver),
                "Error while deserializing field `%s`",
                field->key);
            found = true;
            break;
        }

        if (!found) {
            if (field->on_missing) {
                TRY(field->on_missing(field->target_ptr));
            } else {
                return ERROR(
                    PDF_ERR_MISSING_DICT_KEY,
                    "Missing key `%s`",
                    field->key
                );
            }
        }
    }

    LOG_DIAG(TRACE, DESER, "Finished deserializing dictionary object");

    return NULL;
}

Error* pdf_deserde_operands(
    const PdfObjectVec* operands,
    const PdfOperandDescriptor* descriptors,
    size_t num_descriptors,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(operands);
    RELEASE_ASSERT(descriptors);

    if (num_descriptors > pdf_object_vec_len(operands)) {
        return ERROR(
            PDF_ERR_MISSING_OPERAND,
            "Incorrect number of operands. Expected %zu, found %zu",
            num_descriptors,
            pdf_object_vec_len(operands)
        );
    }

    if (num_descriptors < pdf_object_vec_len(operands)) {
        return ERROR(
            PDF_ERR_EXCESS_OPERAND,
            "Incorrect number of operands. Expected %zu, found %zu",
            num_descriptors,
            pdf_object_vec_len(operands)
        );
    }

    for (size_t idx = 0; idx < num_descriptors; idx++) {
        PdfOperandDescriptor descriptor = descriptors[idx];

        PdfObject operand;
        RELEASE_ASSERT(pdf_object_vec_get(operands, idx, &operand));
        TRY(descriptor.deserializer(&operand, descriptor.target_ptr, resolver));
    }

    return NULL;
}

Error* pdf_deserde_typed_array(
    const PdfObject* object,
    PdfDeserdeFn deserializer,
    PdfResolver* resolver,
    bool allow_single_element,
    void* (*push_uninit)(void* ptr_to_vec_ptr, Arena* arena),
    void** ptr_to_vec_ptr
) {
    LOG_TODO();
}

Error* pdf_deserde_boolean(
    const PdfObject* object,
    PdfBoolean* target_ptr,
    PdfResolver* resolver
) {
    LOG_TODO();
}

Error* pdf_deserde_integer(
    const PdfObject* object,
    PdfInteger* target_ptr,
    PdfResolver* resolver
) {
    LOG_TODO();
}

Error* pdf_deserde_real(
    const PdfObject* object,
    PdfReal* target_ptr,
    PdfResolver* resolver
) {
    LOG_TODO();
}

Error* pdf_deserde_string(
    const PdfObject* object,
    PdfString* target_ptr,
    PdfResolver* resolver
) {
    LOG_TODO();
}

Error* pdf_deserde_name(
    const PdfObject* object,
    PdfName* target_ptr,
    PdfResolver* resolver
) {
    LOG_TODO();
}

Error* pdf_deserde_array(
    const PdfObject* object,
    PdfArray* target_ptr,
    PdfResolver* resolver
) {
    LOG_TODO();
}

Error* pdf_deserde_dict(
    const PdfObject* object,
    PdfDict* target_ptr,
    PdfResolver* resolver
) {
    LOG_TODO();
}

Error* pdf_deserde_stream(
    const PdfObject* object,
    PdfStream* target_ptr,
    PdfResolver* resolver
) {
    LOG_TODO();
}

Error* pdf_deserde_indirect_object(
    const PdfObject* object,
    PdfIndirectObject* target_ptr,
    PdfResolver* resolver
) {
    LOG_TODO();
}

Error* pdf_deserde_indirect_ref(
    const PdfObject* object,
    PdfIndirectRef* target_ptr,
    PdfResolver* resolver
) {
    LOG_TODO();
}

PdfFieldDescriptor pdf_unimplemented_field(const char* key) {
    LOG_TODO();
}

PdfFieldDescriptor pdf_ignored_field(const char* key, PdfObject* target_ptr) {
    LOG_TODO();
}

// TODO: Select less-terrible names for the types in these tests
// #ifdef TEST

// #include "object.h"
// #include "test/test.h"
// #include "test_helpers.h"

// #define DESERIALIZER_IMPL_HELPER() \
//     do { \
//         TRY(pdf_deserde_fields( \
//             object, \
//             fields, \
//             sizeof(fields) / sizeof(PdfFieldDescriptor), \
//             false, \
//             resolver, \
//             "test-object" \
//         )); \
//         return NULL; \
//     } while (0)

// #define DESERIALIZER_TEST_HELPER() \
//     PdfResolver* resolver; \
//     TEST_REQUIRE( \
//         pdf_resolver_new(arena, (uint8_t*)buffer, strlen(buffer), &resolver)
//         \
//     ); \
//     PdfObject object; \
//     TEST_REQUIRE(pdf_resolve_ref( \
//         resolver, \
//         (PdfIndirectRef) {.object_id = 1, .generation = 0}, \
//         &object \
//     ));

// typedef struct {
//     PdfBoolean boolean;
//     PdfInteger integer;
//     PdfReal real;
//     PdfString string;
//     PdfName name;
//     PdfArray array;
//     PdfDict dict;
//     PdfStream stream;
//     PdfIndirectRef indirect_ref;
// } TestDeserObjectsStruct;

// Error* deserde_test_objects_struct(
//     PdfObject* object,
//     TestDeserObjectsStruct* deserialized,
//     PdfResolver* resolver
// ) {
//     PdfFieldDescriptor fields[] = {
//         PDF_FIELD(
//             "Boolean",
//             &deserialized->boolean,
//             PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_BOOLEAN)
//         ),
//         PDF_FIELD(
//             "Integer",
//             &deserialized->integer,
//             PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_INTEGER)
//         ),
//         PDF_FIELD(
//             "Real",
//             &deserialized->real,
//             PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_REAL)
//         ),
//         PDF_FIELD(
//             "String",
//             &deserialized->string,
//             PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_STRING)
//         ),
//         PDF_FIELD(
//             "Name",
//             &deserialized->name,
//             PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_NAME)
//         ),
//         PDF_FIELD(
//             "Array",
//             &deserialized->array,
//             PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_ARRAY)
//         ),
//         PDF_FIELD(
//             "Dict",
//             &deserialized->dict,
//             PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_DICT)
//         ),
//         PDF_FIELD(
//             "Stream",
//             &deserialized->stream,
//             PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_STREAM)
//         ),
//         PDF_FIELD(
//             "IndirectRef",
//             &deserialized->indirect_ref,
//             PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_INDIRECT_REF)
//         )
//     };

//     DESERIALIZER_IMPL_HELPER();
// }

// TEST_FUNC(test_deserde_objects) {
//     Arena* arena = arena_new(1024);
//     const char* objects[] = {
//         "<< /Boolean true /Integer 42 /Real 42.5 /String (test) /Name /Hello
//         /Array [1 2 3] /Dict << /A 1 /B 2 >> /Stream 2 0 R /IndirectRef 1 0 R
//         >>",
//         "<< /Length 8 >> stream\n01234567\nendstream\n"
//     };

//     char* buffer = pdf_construct_deserde_test_doc(
//         objects,
//         2,
//         "<< /Size 3 /Root 404 0 R >>",
//         arena
//     );

//     DESERIALIZER_TEST_HELPER();

//     TestDeserObjectsStruct deserialized;
//     TEST_REQUIRE(deserde_test_objects_struct(&object, &deserialized,
//     resolver));

//     TEST_ASSERT(deserialized.boolean);
//     TEST_ASSERT_EQ(42, deserialized.integer);
//     TEST_ASSERT_EQ(42.5, deserialized.real);
//     TEST_ASSERT_EQ("test", pdf_string_as_cstr(deserialized.string, arena));
//     TEST_ASSERT_EQ("Hello", deserialized.name);

//     TEST_ASSERT(deserialized.array.elements);
//     TEST_ASSERT_EQ((size_t)3,
//     pdf_object_vec_len(deserialized.array.elements)); for (size_t idx = 0;
//     idx < 3; idx++) {
//         PdfObject* element;
//         TEST_ASSERT(
//             pdf_object_vec_get(deserialized.array.elements, idx, &element)
//         );
//         TEST_ASSERT(element);
//         TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_INTEGER,
//         element->type); TEST_ASSERT_EQ((PdfInteger)idx + 1,
//         element->data.integer);
//     }

//     TEST_ASSERT(deserialized.dict.entries);
//     TEST_ASSERT_EQ(
//         (size_t)2,
//         pdf_dict_entry_vec_len(deserialized.dict.entries)
//     );
//     for (size_t idx = 0; idx < 2; idx++) {
//         PdfDictEntry entry;
//         TEST_ASSERT(
//             pdf_dict_entry_vec_get(deserialized.dict.entries, idx, &entry)
//         );

//         TEST_ASSERT(entry.key);
//         TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, entry.key->type);
//         TEST_ASSERT_EQ(idx == 0 ? "A" : "B", entry.key->data.name);

//         TEST_ASSERT(entry.value);
//         TEST_ASSERT_EQ(
//             (PdfObjectType)PDF_OBJECT_TYPE_INTEGER,
//             entry.value->type
//         );
//         TEST_ASSERT_EQ((PdfInteger)idx + 1, entry.value->data.integer);
//     }

//     TEST_ASSERT(deserialized.stream.stream_dict);
//     TEST_ASSERT(deserialized.stream.stream_bytes);
//     TEST_ASSERT_EQ((size_t)8, deserialized.stream.decoded_stream_len);
//     TEST_ASSERT(
//         memcmp(
//             "01234567",
//             deserialized.stream.stream_bytes,
//             deserialized.stream.decoded_stream_len
//         )
//         == 0
//     );

//     TEST_ASSERT_EQ((size_t)1, deserialized.indirect_ref.object_id);
//     TEST_ASSERT_EQ((size_t)0, deserialized.indirect_ref.generation);

//     return TEST_RESULT_PASS;
// }

// typedef struct {
//     PdfName hello;
//     PdfInteger world;
// } TestDeserInnerStruct;

// Error* deserde_test_inner_struct(
//     const PdfObject* object,
//     TestDeserInnerStruct* deserialized,
//     PdfResolver* resolver
// ) {
//     PdfFieldDescriptor fields[] = {
//         PDF_FIELD(
//             "Hello",
//             &deserialized->hello,
//             PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_NAME)
//         ),
//         PDF_FIELD(
//             "World",
//             &deserialized->world,
//             PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_INTEGER)
//         )
//     };

//     DESERIALIZER_IMPL_HELPER();
// }

// DESERDE_IMPL_TRAMPOLINE(
//     deserde_test_inner_struct_untyped,
//     deserde_test_inner_struct
// )

// DESERDE_DECL_RESOLVABLE(
//     TestDeserInnerStructRef,
//     TestDeserInnerStruct,
//     test_deserde_inner_struct_ref_init,
//     test_deserde_resolve_inner_struct
// )

// DESERDE_IMPL_RESOLVABLE(
//     TestDeserInnerStructRef,
//     TestDeserInnerStruct,
//     test_deserde_inner_struct_ref_init,
//     test_deserde_resolve_inner_struct,
//     deserde_test_inner_struct
// )

// typedef struct {
//     TestDeserInnerStructRef reference;
// } TestDeserRefStruct;

// Error* deserde_test_ref_struct(
//     PdfObject* object,
//     TestDeserRefStruct* deserialized,
//     PdfResolver* resolver
// ) {
//     PdfFieldDescriptor fields[] = {PDF_FIELD(
//         "Reference",
//         &deserialized->reference,
//         PDF_DESERDE_RESOLVABLE(test_deserde_inner_struct_ref_init)
//     )};

//     DESERIALIZER_IMPL_HELPER();
// }

// TEST_FUNC(test_deserde_ref) {
//     Arena* arena = arena_new(1024);
//     const char* objects[] = {
//         "<< /Reference 2 0 R >>",
//         "<< /Hello /There /World 42 >>"
//     };

//     char* buffer = pdf_construct_deserde_test_doc(
//         objects,
//         2,
//         "<< /Size 3 /Root 404 0 R >>",
//         arena
//     );

//     DESERIALIZER_TEST_HELPER();

//     TestDeserRefStruct deserialized;
//     TEST_REQUIRE(deserde_test_ref_struct(&object, &deserialized, resolver));

//     TestDeserInnerStruct resolved;
//     TEST_REQUIRE(test_deserde_resolve_inner_struct(
//         deserialized.reference,
//         resolver,
//         &resolved
//     ));

//     TEST_ASSERT_EQ("There", resolved.hello);
//     TEST_ASSERT_EQ(42, resolved.world);

//     return TEST_RESULT_PASS;
// }

// typedef struct {
//     TestDeserInnerStruct inner;
// } TestDeserInlineStruct;

// Error* deserde_test_inline_struct(
//     PdfObject* object,
//     TestDeserInlineStruct* deserialized,
//     PdfResolver* resolver
// ) {
//     PdfFieldDescriptor fields[] = {PDF_FIELD(
//         "Inner",
//         &deserialized->inner,
//         PDF_DESERDE_CUSTOM(deserde_test_inner_struct_untyped)
//     )};

//     DESERIALIZER_IMPL_HELPER();
// }

// TEST_FUNC(test_deserde_inline_struct) {
//     Arena* arena = arena_new(1024);
//     const char* objects[] = {
//         "<< /Inner 2 0 R >>",
//         "<< /Hello /There /World 42 >>"
//     };

//     char* buffer = pdf_construct_deserde_test_doc(
//         objects,
//         2,
//         "<< /Size 3 /Root 404 0 R >>",
//         arena
//     );

//     PdfResolver* resolver;
//     TEST_REQUIRE(
//         pdf_resolver_new(arena, (uint8_t*)buffer, strlen(buffer), &resolver)
//     );

//     PdfObject object;
//     TEST_REQUIRE(pdf_resolve_ref(
//         resolver,
//         (PdfIndirectRef) {.object_id = 1, .generation = 0},
//         &object
//     ));

//     TestDeserInlineStruct deserialized;
//     TEST_REQUIRE(deserde_test_inline_struct(&object, &deserialized,
//     resolver));

//     TEST_ASSERT_EQ("There", deserialized.inner.hello);
//     TEST_ASSERT_EQ(42, deserialized.inner.world);

//     return TEST_RESULT_PASS;
// }

// DESERDE_DECL_OPTIONAL(
//     TestDeserInnerOptional,
//     TestDeserInnerStruct,
//     test_deserde_inner_struct_op_init
// )

// DESERDE_IMPL_OPTIONAL(TestDeserInnerOptional,
// test_deserde_inner_struct_op_init)

// typedef struct {
//     TestDeserInnerOptional inner;
// } TestDeserInlineOptional;

// Error* deserde_test_inline_optional(
//     PdfObject* object,
//     TestDeserInlineOptional* deserialized,
//     PdfResolver* resolver
// ) {
//     PdfFieldDescriptor fields[] = {PDF_FIELD(
//         "Inner",
//         &deserialized->inner,
//         PDF_DESERDE_OPTIONAL(
//             test_deserde_inner_struct_op_init,
//             PDF_DESERDE_CUSTOM(deserde_test_inner_struct_untyped)
//         )
//     )};

//     DESERIALIZER_IMPL_HELPER();
// }

// TEST_FUNC(test_deserde_inline_optional) {
//     Arena* arena = arena_new(1024);
//     const char* objects[] = {
//         "<< /Inner 2 0 R >>",
//         "<< /Hello /There /World 42 >>"
//     };

//     char* buffer = pdf_construct_deserde_test_doc(
//         objects,
//         2,
//         "<< /Size 3 /Root 404 0 R >>",
//         arena
//     );

//     DESERIALIZER_TEST_HELPER();

//     TestDeserInlineOptional deserialized;
//     TEST_REQUIRE(
//         deserde_test_inline_optional(&object, &deserialized, resolver)
//     );

//     TEST_ASSERT(deserialized.inner.has_value);
//     TEST_ASSERT_EQ("There", deserialized.inner.value.hello);
//     TEST_ASSERT_EQ(42, deserialized.inner.value.world);

//     return TEST_RESULT_PASS;
// }

// TEST_FUNC(test_deserde_inline_optional_none) {
//     Arena* arena = arena_new(1024);
//     const char* objects[] = {"<< >>"};

//     char* buffer = pdf_construct_deserde_test_doc(
//         objects,
//         1,
//         "<< /Size 2 /Root 404 0 R >>",
//         arena
//     );

//     DESERIALIZER_TEST_HELPER();

//     TestDeserInlineOptional deserialized = {.inner.has_value = true};
//     TEST_REQUIRE(
//         deserde_test_inline_optional(&object, &deserialized, resolver)
//     );

//     TEST_ASSERT(!deserialized.inner.has_value);

//     return TEST_RESULT_PASS;
// }

// #define DVEC_NAME DeserializeTestStringArray
// #define DVEC_LOWERCASE_NAME deserde_test_string_array
// #define DVEC_TYPE PdfString
// #include "arena/dvec_impl.h"

// #define DVEC_NAME DeserializeTestIntegerArray
// #define DVEC_LOWERCASE_NAME deserde_test_integer_array
// #define DVEC_TYPE PdfInteger
// #include "arena/dvec_impl.h"

// typedef struct {
//     DeserializeTestStringArray* strings;
//     DeserializeTestIntegerArray* integers;
// } TestDeserPrimitiveArrays;

// Error* deserde_test_primitive_arrays(
//     PdfObject* object,
//     PdfResolver* resolver,
//     TestDeserPrimitiveArrays* deserialized
// ) {
//     PdfFieldDescriptor fields[] = {
//         PDF_FIELD(
//             "Strings",
//             &deserialized->strings,
//             PDF_DESERDE_ARRAY(
//                 deserde_test_string_array_push_uninit,
//                 PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_STRING)
//             )
//         ),
//         PDF_FIELD(
//             "Integers",
//             &deserialized->integers,
//             PDF_DESERDE_ARRAY(
//                 deserde_test_integer_array_push_uninit,
//                 PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_INTEGER)
//             )
//         )
//     };

//     DESERIALIZER_IMPL_HELPER();
// }

// TEST_FUNC(test_deserde_primitive_arrays) {
//     Arena* arena = arena_new(1024);
//     const char* objects[] = {
//         "<< /Strings [(Hello) (World)] /Integers [4 5 6] >>"
//     };

//     char* buffer = pdf_construct_deserde_test_doc(
//         objects,
//         1,
//         "<< /Size 2 /Root 404 0 R >>",
//         arena
//     );

//     DESERIALIZER_TEST_HELPER();

//     TestDeserPrimitiveArrays deserialized;
//     TEST_REQUIRE(
//         deserde_test_primitive_arrays(&object, resolver, &deserialized)
//     );

//     TEST_ASSERT(deserialized.strings);
//     TEST_ASSERT_EQ(
//         (size_t)2,
//         deserde_test_string_array_len(deserialized.strings)
//     );

//     PdfString out_str;
//     TEST_ASSERT(
//         deserde_test_string_array_get(deserialized.strings, 0, &out_str)
//     );
//     TEST_ASSERT_EQ("Hello", pdf_string_as_cstr(out_str, arena));
//     TEST_ASSERT(
//         deserde_test_string_array_get(deserialized.strings, 1, &out_str)
//     );
//     TEST_ASSERT_EQ("World", pdf_string_as_cstr(out_str, arena));

//     TEST_ASSERT(deserialized.integers);
//     TEST_ASSERT_EQ(
//         (size_t)3,
//         deserde_test_integer_array_len(deserialized.integers)
//     );

//     PdfInteger out_int;
//     TEST_ASSERT(
//         deserde_test_integer_array_get(deserialized.integers, 0, &out_int)
//     );
//     TEST_ASSERT_EQ((PdfInteger)4, out_int);
//     TEST_ASSERT(
//         deserde_test_integer_array_get(deserialized.integers, 1, &out_int)
//     );
//     TEST_ASSERT_EQ((PdfInteger)5, out_int);
//     TEST_ASSERT(
//         deserde_test_integer_array_get(deserialized.integers, 2, &out_int)
//     );
//     TEST_ASSERT_EQ((PdfInteger)6, out_int);

//     return TEST_RESULT_PASS;
// }

// #define DVEC_NAME DeserializeTestStructArray
// #define DVEC_LOWERCASE_NAME deserde_test_struct_array
// #define DVEC_TYPE TestDeserInnerStruct
// #include "arena/dvec_impl.h"

// typedef struct {
//     DeserializeTestStructArray* inners;
// } TestDeserTestComplexArray;

// Error* deserde_test_complex_array(
//     PdfObject* object,
//     TestDeserTestComplexArray* deserialized,
//     PdfResolver* resolver
// ) {
//     PdfFieldDescriptor fields[] = {PDF_FIELD(
//         "Inners",
//         &deserialized->inners,
//         PDF_DESERDE_ARRAY(
//             deserde_test_struct_array_push_uninit,
//             PDF_DESERDE_CUSTOM(deserde_test_inner_struct_untyped)
//         )
//     )};

//     DESERIALIZER_IMPL_HELPER();
// }

// TEST_FUNC(test_deserde_custom_array) {
//     Arena* arena = arena_new(1024);
//     const char* objects[] = {
//         "<< /Inners [<< /Hello /There /World 42 >> << /Hello /Example /World
//         5 >>] >>"
//     };

//     char* buffer = pdf_construct_deserde_test_doc(
//         objects,
//         1,
//         "<< /Size 2 /Root 404 0 R >>",
//         arena
//     );

//     DESERIALIZER_TEST_HELPER();

//     TestDeserTestComplexArray deserialized;
//     TEST_REQUIRE(deserde_test_complex_array(&object, &deserialized,
//     resolver));

//     TEST_ASSERT(deserialized.inners);
//     TEST_ASSERT_EQ(
//         (size_t)2,
//         deserde_test_struct_array_len(deserialized.inners)
//     );

//     TestDeserInnerStruct element0;
//     TEST_ASSERT(
//         deserde_test_struct_array_get(deserialized.inners, 0, &element0)
//     );
//     TEST_ASSERT_EQ("There", element0.hello);
//     TEST_ASSERT_EQ(42, element0.world);

//     TestDeserInnerStruct element1;
//     TEST_ASSERT(
//         deserde_test_struct_array_get(deserialized.inners, 1, &element1)
//     );
//     TEST_ASSERT_EQ("Example", element1.hello);
//     TEST_ASSERT_EQ(5, element1.world);

//     return TEST_RESULT_PASS;
// }

// #endif // TEST
