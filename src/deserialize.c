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
) {
    RELEASE_ASSERT(field_ptr);
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(doc);

    PdfObject resolved_object;
    PDF_PROPAGATE(resolve_object(doc, object, &resolved_object));

    LOG_TRACE_G(
        "deserde",
        "Deserializing object field with type `%d`",
        resolved_object.type
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

    switch (field_data.element_info->kind) {
        case PDF_FIELD_KIND_OBJECT: {
            PDF_PROPAGATE(deserialize_object_field(
                *deserialized,
                element,
                field_data.element_info->data.object,
                doc
            ));
            break;
        }
        case PDF_FIELD_KIND_REF: {
            PDF_PROPAGATE(deserialize_ref_field(
                *deserialized,
                element,
                field_data.element_info->data.ref
            ));
            break;
        }
        default: {
            LOG_PANIC("Unreachable");
        }
    }

    return PDF_OK;
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

    void* data_ptr = (char*)field_ptr + field_data.data_offset;
    LOG_INFO(
        "Data offset: %zu (%p), Discriminant offset: %zu (%p)",
        field_data.data_offset,
        data_ptr,
        field_data.discriminant_offset,
        discriminant_ptr
    );
    switch (field_data.inner_info->kind) {
        case PDF_FIELD_KIND_OBJECT: {
            PdfResult result = deserialize_object_field(
                data_ptr,
                object,
                field_data.inner_info->data.object,
                doc
            );

            if (result != PDF_OK) {
                return result;
            }

            break;
        }
        case PDF_FIELD_KIND_REF: {
            PdfResult result = deserialize_ref_field(
                data_ptr,
                object,
                field_data.inner_info->data.ref
            );

            if (result != PDF_OK) {
                return result;
            }

            break;
        }
        case PDF_FIELD_KIND_ARRAY: {
            PdfResult result = deserialize_array_field(
                data_ptr,
                object,
                field_data.inner_info->data.array,
                doc
            );

            if (result != PDF_OK) {
                return result;
            }

            break;
        }
        case PDF_FIELD_KIND_AS_ARRAY: {
            PdfResult result = deserialize_as_array_field(
                data_ptr,
                object,
                field_data.inner_info->data.array,
                doc
            );

            if (result != PDF_OK) {
                return result;
            }

            break;
        }
        default: {
            LOG_PANIC("Unreachable");
        }
    }

    return PDF_OK;
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

            switch (field->info.kind) {
                case PDF_FIELD_KIND_OBJECT: {
                    PDF_PROPAGATE(deserialize_object_field(
                        (char*)target + field->offset,
                        entry->value,
                        field->info.data.object,
                        doc
                    ));
                    break;
                }
                case PDF_FIELD_KIND_REF: {
                    PDF_PROPAGATE(deserialize_ref_field(
                        (char*)target + field->offset,
                        entry->value,
                        field->info.data.ref
                    ));
                    break;
                }
                case PDF_FIELD_KIND_ARRAY: {
                    PDF_PROPAGATE(deserialize_array_field(
                        (char*)target + field->offset,
                        entry->value,
                        field->info.data.array,
                        doc
                    ));
                    break;
                }
                case PDF_FIELD_KIND_AS_ARRAY: {
                    PDF_PROPAGATE(deserialize_as_array_field(
                        (char*)target + field->offset,
                        entry->value,
                        field->info.data.array,
                        doc
                    ));
                    break;
                }
                case PDF_FIELD_KIND_OPTIONAL: {
                    PDF_PROPAGATE(deserialize_optional_field(
                        (char*)target + field->offset,
                        entry->value,
                        field->info.data.optional,
                        doc
                    ));
                    break;
                }
                case PDF_FIELD_KIND_IGNORED: {
                    break;
                }
            }

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
