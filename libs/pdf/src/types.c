#include "pdf/types.h"

#include <math.h>

#include "err/error.h"
#include "geom/mat3.h"
#include "logger/log.h"
#include "pdf/deserde.h"
#include "pdf/object.h"
#include "pdf/resolver.h"

Error* pdf_deserde_boolean_optional_trampoline(
    const PdfObject* object,
    void* target_ptr,
    PdfResolver* resolver
) {
    PdfBooleanOptional* target = target_ptr;
    target->is_some = 1;
    return pdf_deserde_boolean(object, (PdfBoolean*)&target->value, resolver);
}
void pdf_boolean_init_none(void* target_ptr) {
    PdfBooleanOptional* target = target_ptr;
    target->is_some = 0;
}
inline PdfFieldDescriptor
pdf_boolean_optional_field(const char* key, PdfBooleanOptional* target_ptr) {
    return (PdfFieldDescriptor) {.key = key,
                                 .target_ptr = (void*)target_ptr,
                                 .deserializer =
                                     pdf_deserde_boolean_optional_trampoline,
                                 .init_none = pdf_boolean_init_none};
}
PDF_IMPL_OPTIONAL_FIELD(PdfInteger, PdfIntegerOptional, integer)
PDF_IMPL_OPTIONAL_FIELD(PdfReal, PdfRealOptional, real)
PDF_IMPL_OPTIONAL_FIELD(PdfString, PdfStringOptional, string)
PDF_IMPL_OPTIONAL_FIELD(PdfName, PdfNameOptional, name)
PDF_IMPL_OPTIONAL_FIELD(PdfArray, PdfArrayOptional, array)
PDF_IMPL_OPTIONAL_FIELD(PdfDict, PdfDictOptional, dict)
PDF_IMPL_OPTIONAL_FIELD(PdfStream, PdfStreamOptional, stream)
PDF_IMPL_OPTIONAL_FIELD(
    PdfIndirectObject,
    PdfIndirectObjectOptional,
    indirect_object
)
PDF_IMPL_OPTIONAL_FIELD(PdfIndirectRef, PdfIndirectRefOptional, indirect_ref)

Error* pdf_deserde_number(
    const PdfObject* object,
    PdfNumber* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    (void)resolver;

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
            return ERROR(
                PDF_ERR_INCORRECT_TYPE,
                "Numbers must be either integers or reals"
            );
        }
    }

    return NULL;
}

DESERDE_IMPL_TRAMPOLINE(pdf_deserde_number_trampoline, pdf_deserde_number)
DESERDE_IMPL_OPTIONAL(PdfNumberOptional, pdf_number_op_init)

Error* pdf_deserde_num_as_real(
    const PdfObject* object,
    PdfReal* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    PdfNumber num;
    TRY(pdf_deserde_number(object, &num, resolver));
    *target_ptr = pdf_number_as_real(num);

    return NULL;
}

#define DVEC_NAME PdfNumberVec
#define DVEC_LOWERCASE_NAME pdf_number_vec
#define DVEC_TYPE PdfNumber
#include "arena/dvec_impl.h"

DESERDE_IMPL_OPTIONAL(PdfNumberVecOptional, pdf_number_vec_op_init)

DESERDE_IMPL_TRAMPOLINE(
    pdf_deserde_num_as_real_trampoline,
    pdf_deserde_num_as_real
)

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

PdfObject pdf_number_as_object(PdfNumber number) {
    if (number.type == PDF_NUMBER_TYPE_INTEGER) {
        return (PdfObject) {.type = PDF_OBJECT_TYPE_INTEGER,
                            .data.integer = number.value.integer};
    } else {
        return (PdfObject) {.type = PDF_OBJECT_TYPE_REAL,
                            .data.real = number.value.real};
    }
}

int pdf_number_cmp(PdfNumber lhs, PdfNumber rhs) {
    if (lhs.type == PDF_NUMBER_TYPE_INTEGER
        && rhs.type == PDF_NUMBER_TYPE_INTEGER) {
        if (lhs.value.integer < rhs.value.integer) {
            return -1;
        } else if (lhs.value.integer > rhs.value.integer) {
            return 1;
        } else {
            return 0;
        }
    }

    PdfReal lhs_real = pdf_number_as_real(lhs);
    PdfReal rhs_real = pdf_number_as_real(rhs);

    if (fabs(lhs_real - rhs_real) < 1e-6) {
        return 0;
    } else if (lhs_real < rhs_real) {
        return -1;
    } else {
        return 1;
    }
}

Error* pdf_deserde_geom_vec3(
    const PdfObject* object,
    GeomVec3* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    switch (object->type) {
        case PDF_OBJECT_TYPE_ARRAY: {
            if (pdf_object_vec_len(object->data.array.elements) != 3) {
                return ERROR(
                    PDF_ERR_INCORRECT_TYPE,
                    "Incorrect number of elements in vec array (expected 3)"
                );
            }

            PdfReal array[3] = {0};
            for (size_t idx = 0; idx < 3; idx++) {
                PdfObject* element = NULL;
                RELEASE_ASSERT(pdf_object_vec_get(
                    object->data.array.elements,
                    idx,
                    &element
                ));

                PdfNumber number;
                TRY(pdf_deserde_number(element, &number, resolver));

                array[idx] = pdf_number_as_real(number);
            }

            *target_ptr = geom_vec3_new(array[0], array[1], array[2]);
            break;
        }
        default: {
            return ERROR(PDF_ERR_INCORRECT_TYPE, "Vec must be an array");
        }
    }

    return NULL;
}

DESERDE_IMPL_TRAMPOLINE(pdf_deserde_geom_vec3_trampoline, pdf_deserde_geom_vec3)
DESERDE_IMPL_OPTIONAL(PdfGeomVec3Optional, pdf_geom_vec3_op_init)

Error* pdf_deserde_rectangle(
    const PdfObject* object,
    PdfRectangle* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);

    switch (object->type) {
        case PDF_OBJECT_TYPE_ARRAY: {
            if (pdf_object_vec_len(object->data.array.elements) != 4) {
                return ERROR(
                    PDF_ERR_INCORRECT_TYPE,
                    "Incorrect number of elements in rectangle array (expected 4)"
                );
            }

            PdfObject* ll_x = NULL;
            RELEASE_ASSERT(
                pdf_object_vec_get(object->data.array.elements, 0, &ll_x)
            );
            TRY(pdf_deserde_number(ll_x, &target_ptr->lower_left_x, resolver));

            PdfObject* ll_y = NULL;
            RELEASE_ASSERT(
                pdf_object_vec_get(object->data.array.elements, 1, &ll_y)
            );
            TRY(pdf_deserde_number(ll_y, &target_ptr->lower_left_y, resolver));

            PdfObject* ur_x = NULL;
            RELEASE_ASSERT(
                pdf_object_vec_get(object->data.array.elements, 2, &ur_x)
            );
            TRY(pdf_deserde_number(ur_x, &target_ptr->upper_right_x, resolver));

            PdfObject* ur_y = NULL;
            RELEASE_ASSERT(
                pdf_object_vec_get(object->data.array.elements, 3, &ur_y)
            );
            TRY(pdf_deserde_number(ur_y, &target_ptr->upper_right_y, resolver));
            break;
        }
        default: {
            return ERROR(PDF_ERR_INCORRECT_TYPE, "Rectangle must be an array");
        }
    }

    return NULL;
}

DESERDE_IMPL_TRAMPOLINE(pdf_deserde_rectangle_trampoline, pdf_deserde_rectangle)
DESERDE_IMPL_OPTIONAL(PdfRectangleOptional, pdf_rectangle_op_init)

Error* pdf_deserde_pdf_mat(
    const PdfObject* object,
    GeomMat3* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    switch (object->type) {
        case PDF_OBJECT_TYPE_ARRAY: {
            if (pdf_object_vec_len(object->data.array.elements) != 6) {
                return ERROR(
                    PDF_ERR_INCORRECT_TYPE,
                    "Incorrect number of elements in matrix array (expected 6)"
                );
            }

            PdfReal array[6] = {0};
            for (size_t idx = 0; idx < 6; idx++) {
                PdfObject* element = NULL;
                RELEASE_ASSERT(pdf_object_vec_get(
                    object->data.array.elements,
                    idx,
                    &element
                ));

                PdfNumber number;
                TRY(pdf_deserde_number(element, &number, resolver));

                array[idx] = pdf_number_as_real(number);
            }

            *target_ptr = geom_mat3_new_pdf(
                array[0],
                array[1],
                array[2],
                array[3],
                array[4],
                array[5]
            );
            break;
        }
        default: {
            return ERROR(PDF_ERR_INCORRECT_TYPE, "Matrix must be an array");
        }
    }

    return NULL;
}

DESERDE_IMPL_TRAMPOLINE(pdf_deserde_pdf_mat_trampoline, pdf_deserde_pdf_mat)

DESERDE_IMPL_OPTIONAL(PdfGeomMat3Optional, pdf_geom_mat3_op_init)

Error* pdf_deserde_geom_mat3(
    const PdfObject* object,
    GeomMat3* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    switch (object->type) {
        case PDF_OBJECT_TYPE_ARRAY: {
            if (pdf_object_vec_len(object->data.array.elements) != 9) {
                return ERROR(
                    PDF_ERR_INCORRECT_TYPE,
                    "Incorrect number of elements in matrix array (expected 9)"
                );
            }

            PdfReal array[9] = {0};
            for (size_t idx = 0; idx < 9; idx++) {
                PdfObject* element = NULL;
                RELEASE_ASSERT(pdf_object_vec_get(
                    object->data.array.elements,
                    idx,
                    &element
                ));

                PdfNumber number;
                TRY(pdf_deserde_number(element, &number, resolver));

                array[idx] = pdf_number_as_real(number);
            }

            *target_ptr = geom_mat3_new(
                array[0],
                array[1],
                array[2],
                array[3],
                array[4],
                array[5],
                array[6],
                array[7],
                array[8]
            );
            break;
        }
        default: {
            return ERROR(PDF_ERR_INCORRECT_TYPE, "Matrix must be an array");
        }
    }

    return NULL;
}

DESERDE_IMPL_TRAMPOLINE(pdf_deserde_geom_mat3_trampoline, pdf_deserde_geom_mat3)

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
