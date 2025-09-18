// TODO: Since PdfNumber has been moved to object, this should probably move to,
// but I'm not sure if it will stay in object or not.
#include "arena/arena.h"
#include "deserialize.h"
#include "logger/log.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"

PdfError* pdf_deserialize_number(
    const PdfObject* object,
    PdfNumber* target_ptr,
    PdfOptionalResolver resolver,
    Arena* arena
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    (void)resolver;
    (void)arena;

    switch (object->type) {
        case PDF_OBJECT_TYPE_INTEGER: {
            target_ptr->type = PDF_NUMBER_TYPE_INTEGER;
            target_ptr->value.integer = object->data.integer;
            break;
        }
        case PDF_OBJECT_TYPE_REAL: {
            target_ptr->type = PDF_NUMBER_TYPE_REAL;
            target_ptr->value.real = object->data.real;
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

DESERDE_IMPL_TRAMPOLINE(
    pdf_deserialize_number_trampoline,
    pdf_deserialize_number
)
DESERDE_IMPL_OPTIONAL(PdfNumberOptional, pdf_number_op_init)

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

PdfError* pdf_deserialize_rectangle(
    const PdfObject* object,
    PdfRectangle* target_ptr,
    PdfOptionalResolver resolver,
    Arena* arena
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);

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
            PDF_PROPAGATE(pdf_deserialize_number(
                ll_x,
                &target_ptr->lower_left_x,
                resolver,
                arena
            ));

            PdfObject* ll_y = NULL;
            RELEASE_ASSERT(
                pdf_object_vec_get(object->data.array.elements, 1, &ll_y)
            );
            PDF_PROPAGATE(pdf_deserialize_number(
                ll_y,
                &target_ptr->lower_left_y,
                resolver,
                arena
            ));

            PdfObject* ur_x = NULL;
            RELEASE_ASSERT(
                pdf_object_vec_get(object->data.array.elements, 2, &ur_x)
            );
            PDF_PROPAGATE(pdf_deserialize_number(
                ur_x,
                &target_ptr->upper_right_x,
                resolver,
                arena
            ));

            PdfObject* ur_y = NULL;
            RELEASE_ASSERT(
                pdf_object_vec_get(object->data.array.elements, 3, &ur_y)
            );
            PDF_PROPAGATE(pdf_deserialize_number(
                ur_y,
                &target_ptr->upper_right_y,
                resolver,
                arena
            ));
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

DESERDE_IMPL_TRAMPOLINE(
    pdf_deserialize_rectangle_trampoline,
    pdf_deserialize_rectangle
)
DESERDE_IMPL_OPTIONAL(PdfRectangleOptional, pdf_rectangle_op_init)

#define DVEC_NAME PdfNameVec
#define DVEC_LOWERCASE_NAME pdf_name_vec
#define DVEC_TYPE PdfName
#include "arena/dvec_impl.h"
DESERDE_IMPL_OPTIONAL(PdfNameVecOptional, pdf_name_vec_op_init)

DESERDE_IMPL_OPTIONAL(PdfBooleanOptional, pdf_boolean_op_init)
DESERDE_IMPL_OPTIONAL(PdfIntegerOptional, pdf_integer_op_init)
DESERDE_IMPL_OPTIONAL(PdfRealOptional, pdf_real_op_init)
DESERDE_IMPL_OPTIONAL(PdfStringOptional, pdf_string_op_init)
DESERDE_IMPL_OPTIONAL(PdfNameOptional, pdf_name_op_init)
DESERDE_IMPL_OPTIONAL(PdfArrayOptional, pdf_array_op_init)
DESERDE_IMPL_OPTIONAL(PdfDictOptional, pdf_dict_op_init)
DESERDE_IMPL_OPTIONAL(PdfStreamOptional, pdf_stream_op_init)
DESERDE_IMPL_OPTIONAL(PdfIndirectObjectOptional, pdf_indirect_object_op_init)
DESERDE_IMPL_OPTIONAL(PdfIndirectRefOptional, pdf_indirect_ref_op_init)
