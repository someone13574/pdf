#include "pdf/types.h"

#include <math.h>

#include "err/error.h"
#include "geom/mat3.h"
#include "logger/log.h"
#include "pdf/deserde.h"
#include "pdf/object.h"
#include "pdf/resolver.h"

PDF_IMPL_FIELD(PdfBoolean, boolean)
PDF_IMPL_FIELD(PdfInteger, integer)
PDF_IMPL_FIELD(PdfReal, real)
PDF_IMPL_FIELD(PdfString, string)
PDF_IMPL_FIELD(PdfName, name)
PDF_IMPL_FIELD(PdfArray, array)
PDF_IMPL_FIELD(PdfDict, dict)
PDF_IMPL_FIELD(PdfStream, stream)
PDF_IMPL_FIELD(PdfIndirectObject, indirect_object)
PDF_IMPL_FIELD(PdfIndirectRef, indirect_ref)

PDF_IMPL_OPTIONAL_FIELD(PdfBoolean, PdfBooleanOptional, boolean)
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

#define DVEC_NAME PdfBooleanVec
#define DVEC_LOWERCASE_NAME pdf_boolean_vec
#define DVEC_TYPE PdfBoolean
#include "arena/dvec_impl.h"

PDF_IMPL_ARRAY_FIELD(PdfBooleanVec, boolean_vec, boolean)
PDF_IMPL_FIXED_ARRAY_FIELD(PdfBoolean, boolean)
PDF_IMPL_OPTIONAL_FIELD(PdfBooleanVec*, PdfBooleanVecOptional, boolean_vec)

#define DVEC_NAME PdfNameVec
#define DVEC_LOWERCASE_NAME pdf_name_vec
#define DVEC_TYPE PdfName
#include "arena/dvec_impl.h"

PDF_IMPL_ARRAY_FIELD(PdfNameVec, name_vec, name)
PDF_IMPL_AS_ARRAY_FIELD(PdfNameVec, name_vec, name)
PDF_IMPL_OPTIONAL_FIELD(PdfNameVec*, PdfNameVecOptional, name_vec)
PDF_IMPL_OPTIONAL_FIELD(PdfNameVec*, PdfAsNameVecOptional, as_name_vec)

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

PDF_IMPL_FIELD(PdfNumber, number)
PDF_IMPL_FIELD(PdfReal, num_as_real)
PDF_IMPL_OPTIONAL_FIELD(PdfNumber, PdfNumberOptional, number)
PDF_IMPL_OPTIONAL_FIELD(PdfReal, PdfNumAsRealOptional, num_as_real)

#define DVEC_NAME PdfNumberVec
#define DVEC_LOWERCASE_NAME pdf_number_vec
#define DVEC_TYPE PdfNumber
#include "arena/dvec_impl.h"

PDF_IMPL_ARRAY_FIELD(PdfNumberVec, number_vec, number)
PDF_IMPL_FIXED_ARRAY_FIELD(PdfNumber, number)
PDF_IMPL_OPTIONAL_FIELD(PdfNumberVec*, PdfNumberVecOptional, number_vec)

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

            PdfObject ll_x;
            RELEASE_ASSERT(
                pdf_object_vec_get(object->data.array.elements, 0, &ll_x)
            );
            TRY(pdf_deserde_number(&ll_x, &target_ptr->lower_left_x, resolver));

            PdfObject ll_y;
            RELEASE_ASSERT(
                pdf_object_vec_get(object->data.array.elements, 1, &ll_y)
            );
            TRY(pdf_deserde_number(&ll_y, &target_ptr->lower_left_y, resolver));

            PdfObject ur_x;
            RELEASE_ASSERT(
                pdf_object_vec_get(object->data.array.elements, 2, &ur_x)
            );
            TRY(
                pdf_deserde_number(&ur_x, &target_ptr->upper_right_x, resolver)
            );

            PdfObject ur_y;
            RELEASE_ASSERT(
                pdf_object_vec_get(object->data.array.elements, 3, &ur_y)
            );
            TRY(
                pdf_deserde_number(&ur_y, &target_ptr->upper_right_y, resolver)
            );
            break;
        }
        default: {
            return ERROR(PDF_ERR_INCORRECT_TYPE, "Rectangle must be an array");
        }
    }

    return NULL;
}

PDF_IMPL_FIELD(PdfRectangle, rectangle)
PDF_IMPL_OPTIONAL_FIELD(PdfRectangle, PdfRectangleOptional, rectangle)

Error* pdf_deserde_geom_vec2(
    const PdfObject* object,
    GeomVec2* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    switch (object->type) {
        case PDF_OBJECT_TYPE_ARRAY: {
            if (pdf_object_vec_len(object->data.array.elements) != 2) {
                return ERROR(
                    PDF_ERR_INCORRECT_TYPE,
                    "Incorrect number of elements in vec array (expected 2)"
                );
            }

            PdfReal array[2] = {0};
            for (size_t idx = 0; idx < 2; idx++) {
                PdfObject element;
                RELEASE_ASSERT(pdf_object_vec_get(
                    object->data.array.elements,
                    idx,
                    &element
                ));

                PdfNumber number;
                TRY(pdf_deserde_number(&element, &number, resolver));

                array[idx] = pdf_number_as_real(number);
            }

            *target_ptr = geom_vec2_new(array[0], array[1]);
            break;
        }
        default: {
            return ERROR(PDF_ERR_INCORRECT_TYPE, "Vec must be an array");
        }
    }

    return NULL;
}

PDF_IMPL_FIELD(GeomVec2, geom_vec2)
PDF_IMPL_OPTIONAL_FIELD(GeomVec2, PdfGeomVec2Optional, geom_vec2)

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
                PdfObject element;
                RELEASE_ASSERT(pdf_object_vec_get(
                    object->data.array.elements,
                    idx,
                    &element
                ));

                PdfNumber number;
                TRY(pdf_deserde_number(&element, &number, resolver));

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

PDF_IMPL_FIELD(GeomVec3, geom_vec3)
PDF_IMPL_OPTIONAL_FIELD(GeomVec3, PdfGeomVec3Optional, geom_vec3)

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
                PdfObject element;
                RELEASE_ASSERT(pdf_object_vec_get(
                    object->data.array.elements,
                    idx,
                    &element
                ));

                PdfNumber number;
                TRY(pdf_deserde_number(&element, &number, resolver));

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
                PdfObject element;
                RELEASE_ASSERT(pdf_object_vec_get(
                    object->data.array.elements,
                    idx,
                    &element
                ));

                PdfNumber number;
                TRY(pdf_deserde_number(&element, &number, resolver));

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

PDF_IMPL_FIELD(GeomMat3, geom_mat3)
PDF_IMPL_FIELD(GeomMat3, pdf_mat)
PDF_IMPL_OPTIONAL_FIELD(GeomMat3, PdfGeomMat3Optional, geom_mat3)
PDF_IMPL_OPTIONAL_FIELD(GeomMat3, PdfGeomPdfMatOptional, pdf_mat)
