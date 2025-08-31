#include "pdf/types.h"

#include "logger/log.h"
#include "pdf/object.h"
#include "pdf_error/error.h"

PdfError* pdf_deserialize_number(PdfObject* object, PdfNumber* deserialized) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(deserialized);

    switch (object->type) {
        case PDF_OBJECT_TYPE_INTEGER: {
            deserialized->type = PDF_NUMBER_TYPE_INTEGER;
            deserialized->value.integer = object->data.integer;
            break;
        }
        case PDF_OBJECT_TYPE_REAL: {
            deserialized->type = PDF_NUMBER_TYPE_REAL;
            deserialized->value.real = object->data.real;
            break;
        }
        default: {
            return PDF_ERROR(
                PDF_ERR_INCORRECT_TYPE,
                "Numbers must be either integers or reals"
            );
        }
    }

    return NULL;
}

PdfError* pdf_deserialize_number_wrapper(
    PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    void* deserialized
) {
    (void)arena;
    (void)resolver;

    return pdf_deserialize_number(object, deserialized);
}

PdfReal pdf_number_as_real(PdfNumber number) {
    switch (number.type) {
        case PDF_NUMBER_TYPE_INTEGER: {
            return number.value.integer;
        }
        case PDF_NUMBER_TYPE_REAL: {
            return number.value.real;
        }
        default: {
            LOG_PANIC("Unreachable");
        }
    }
}

PdfError*
pdf_deserialize_rectangle(PdfObject* object, PdfRectangle* deserialized) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(deserialized);

    switch (object->type) {
        case PDF_OBJECT_TYPE_ARRAY: {
            if (pdf_object_vec_len(object->data.array.elements) != 4) {
                return PDF_ERROR(
                    PDF_ERR_INCORRECT_TYPE,
                    "Incorrect number of elements in rectangle array (expected 4)"
                );
            }

            PdfObject* ll_x = NULL;
            RELEASE_ASSERT(
                pdf_object_vec_get(object->data.array.elements, 0, &ll_x)
            );
            PDF_PROPAGATE(
                pdf_deserialize_number(ll_x, &deserialized->lower_left_x)
            );

            PdfObject* ll_y = NULL;
            RELEASE_ASSERT(
                pdf_object_vec_get(object->data.array.elements, 1, &ll_y)
            );
            PDF_PROPAGATE(
                pdf_deserialize_number(ll_y, &deserialized->lower_left_y)
            );

            PdfObject* ur_x = NULL;
            RELEASE_ASSERT(
                pdf_object_vec_get(object->data.array.elements, 2, &ur_x)
            );
            PDF_PROPAGATE(
                pdf_deserialize_number(ur_x, &deserialized->upper_right_x)
            );

            PdfObject* ur_y = NULL;
            RELEASE_ASSERT(
                pdf_object_vec_get(object->data.array.elements, 3, &ur_y)
            );
            PDF_PROPAGATE(
                pdf_deserialize_number(ur_y, &deserialized->upper_right_y)
            );
            break;
        }
        default: {
            return PDF_ERROR(
                PDF_ERR_INCORRECT_TYPE,
                "Rectangles must be an array"
            );
        }
    }

    return NULL;
}

PdfError* pdf_deserialize_rectangle_wrapper(
    PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    void* deserialized
) {
    (void)arena;
    (void)resolver;

    return pdf_deserialize_rectangle(object, deserialized);
}
