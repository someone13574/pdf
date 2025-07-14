#include "deserialize.h"

#include <string.h>

#include "log.h"
#include "pdf_object.h"
#include "pdf_result.h"
#include "vec.h"

void deserialize_object_field(void* field_ptr, PdfObject* object) {
    LOG_TRACE_G(
        "deserde",
        "Deserializing object field with type `%d`",
        object->type
    );

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
}

void deserialize_op_object_field(
    void* field_ptr,
    PdfFieldDataOpObject field_data,
    PdfObject* object
) {
    LOG_TRACE_G(
        "deserde",
        "Deserializing optional object field with type `%d`",
        object->type
    );

    bool has_value = true;
    memcpy(
        (char*)field_ptr + field_data.discriminant_offset,
        &has_value,
        sizeof(has_value)
    );

    deserialize_object_field(
        (char*)field_ptr + field_data.value_offset,
        object
    );
}

void object_field_none(void* field_ptr, PdfFieldDataOpObject field_data) {
    LOG_TRACE_G("deserde", "Optional object field is none");

    bool has_value = false;
    memcpy(
        (char*)field_ptr + field_data.discriminant_offset,
        &has_value,
        sizeof(has_value)
    );
}

void deserialize_ref_field(
    void* field_ptr,
    PdfFieldDataRef field_data,
    PdfObject* object
) {
    LOG_TRACE_G("deserde", "Deserializing ref field");

    void* resolved_field = (char*)field_ptr + field_data.resolved_offset;
    memset(resolved_field, 0, sizeof(void*));

    void* ref_field = (char*)field_ptr + field_data.ref_offset;
    PdfIndirectRef ref = object->data.indirect_ref;
    memcpy(ref_field, &ref, sizeof(PdfIndirectRef));
}

void deserialize_op_ref_field(
    void* field_offset,
    PdfFieldDataOpRef data,
    PdfObject* object
) {
    LOG_TRACE_G("deserde", "Deserializing optional ref field");

    bool has_value = true;
    memcpy(
        (char*)field_offset + data.discriminant_offset,
        &has_value,
        sizeof(has_value)
    );

    deserialize_object_field((char*)field_offset + data.data_offset, object);
}

void ref_field_none(void* field_offset, PdfFieldDataOpRef data) {
    LOG_TRACE_G("deserde", "Optional ref field is none");

    bool has_value = false;
    memcpy(
        (char*)field_offset + data.discriminant_offset,
        &has_value,
        sizeof(has_value)
    );
}

PdfResult pdf_deserialize_object(
    void* target,
    PdfFieldDescriptor* fields,
    size_t num_fields,
    PdfObject* object
) {
    if (!target || !fields || !object) {
        return PDF_ERR_NULL_ARGS;
    }

    // Check object type
    if (object->type == PDF_OBJECT_TYPE_INDIRECT_OBJECT) {
        object = object->data.indirect_object.object;
    }

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
                found = true;
                break;
            }
        }

        if (!found) {
            LOG_ERROR_G(
                "deserde",
                "Dict key `%s` is not a known field",
                entry->key->data.name
            );
            return PDF_ERR_SCHEMA_UNKNOWN_KEY;
        }
    }

    // Deserialize fields
    for (size_t field_idx = 0; field_idx < num_fields; field_idx++) {
        bool found = false;
        PdfFieldDescriptor* field = &fields[field_idx];
        LOG_TRACE_G("deserde", "Field: `%s`", field->key);

        for (size_t entry_idx = 0;
             entry_idx < vec_len(object->data.dict.entries);
             entry_idx++) {
            PdfDictEntry* entry = vec_get(object->data.dict.entries, entry_idx);
            if (strcmp(entry->key->data.name, fields[field_idx].key) != 0) {
                continue;
            }

            switch (field->kind) {
                case PDF_FIELD_KIND_OBJECT: {
                    if (entry->value->type != field->data.object.type) {
                        LOG_ERROR_G(
                            "deserde",
                            "Incorrect type for object field `%s`",
                            field->key
                        );
                        return PDF_ERR_SCHEMA_INCORRECT_TYPE;
                    }

                    deserialize_object_field(
                        (char*)target + field->offset,
                        entry->value
                    );
                    break;
                }
                case PDF_FIELD_KIND_OP_OBJECT: {
                    if (entry->value->type != field->data.object.type) {
                        LOG_ERROR_G(
                            "deserde",
                            "Incorrect type for optional object field `%s`",
                            field->key
                        );
                        return PDF_ERR_SCHEMA_INCORRECT_TYPE;
                    }

                    deserialize_op_object_field(
                        (char*)target + field->offset,
                        field->data.op_object,
                        entry->value
                    );
                    break;
                }
                case PDF_FIELD_KIND_REF: {
                    if (entry->value->type != PDF_OBJECT_TYPE_INDIRECT_REF) {
                        LOG_ERROR_G(
                            "deserde",
                            "Struct ref field `%s` not a indirect ref",
                            field->key
                        );
                        return PDF_ERR_SCHEMA_INCORRECT_TYPE;
                    }

                    deserialize_ref_field(
                        (char*)target + field->offset,
                        field->data.ref,
                        entry->value
                    );
                    break;
                }
                case PDF_FIELD_KIND_OP_REF: {
                    if (entry->value->type != PDF_OBJECT_TYPE_INDIRECT_REF) {
                        LOG_ERROR_G(
                            "deserde",
                            "Optional struct ref field `%s` not a indirect ref",
                            field->key
                        );
                        return PDF_ERR_SCHEMA_INCORRECT_TYPE;
                    }

                    deserialize_op_ref_field(
                        (char*)target + field->offset,
                        field->data.op_ref,
                        entry->value
                    );
                    break;
                }
            }

            found = true;
            break;
        }

        if (!found) {
            switch (field->kind) {
                case PDF_FIELD_KIND_OBJECT:
                case PDF_FIELD_KIND_REF: {
                    return PDF_ERR_MISSING_DICT_KEY;
                }
                case PDF_FIELD_KIND_OP_OBJECT: {
                    object_field_none(
                        (char*)target + field->offset,
                        field->data.op_object
                    );
                    break;
                }
                case PDF_FIELD_KIND_OP_REF: {
                    ref_field_none(
                        (char*)target + field->offset,
                        field->data.op_ref
                    );
                    break;
                }
            }
        }
    }

    LOG_TRACE_G("deserde", "Finished deserializing dictionary object");

    return PDF_OK;
}
