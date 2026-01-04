#pragma once

#include "geom/mat3.h"
#include "geom/vec2.h"
#include "geom/vec3.h"
#include "pdf/deserde.h"
#include "pdf/object.h"
#include "pdf/resolver.h"

PDF_DECL_FIELD(PdfBoolean, boolean)
PDF_DECL_FIELD(PdfInteger, integer)
PDF_DECL_FIELD(PdfReal, real)
PDF_DECL_FIELD(PdfString, string)
PDF_DECL_FIELD(PdfName, name)
PDF_DECL_FIELD(PdfArray, array)
PDF_DECL_FIELD(PdfDict, dict)
PDF_DECL_FIELD(PdfStream, stream)
PDF_DECL_FIELD(PdfIndirectObject, indirect_object)
PDF_DECL_FIELD(PdfIndirectRef, indirect_ref)

PDF_DECL_OPTIONAL_FIELD(PdfBoolean, PdfBooleanOptional, boolean)
PDF_DECL_OPTIONAL_FIELD(PdfInteger, PdfIntegerOptional, integer)
PDF_DECL_OPTIONAL_FIELD(PdfReal, PdfRealOptional, real)
PDF_DECL_OPTIONAL_FIELD(PdfString, PdfStringOptional, string)
PDF_DECL_OPTIONAL_FIELD(PdfName, PdfNameOptional, name)
PDF_DECL_OPTIONAL_FIELD(PdfArray, PdfArrayOptional, array)
PDF_DECL_OPTIONAL_FIELD(PdfDict, PdfDictOptional, dict)
PDF_DECL_OPTIONAL_FIELD(PdfStream, PdfStreamOptional, stream)
PDF_DECL_OPTIONAL_FIELD(
    PdfIndirectObject,
    PdfIndirectObjectOptional,
    indirect_object
)
PDF_DECL_OPTIONAL_FIELD(PdfIndirectRef, PdfIndirectRefOptional, indirect_ref)

#define DVEC_NAME PdfBooleanVec
#define DVEC_LOWERCASE_NAME pdf_boolean_vec
#define DVEC_TYPE PdfBoolean
#include "arena/dvec_decl.h"

PDF_DECL_ARRAY_FIELD(PdfBooleanVec, boolean_vec)
PDF_DECL_FIXED_ARRAY_FIELD(PdfBoolean, boolean)
PDF_DECL_OPTIONAL_FIELD(PdfBooleanVec*, PdfBooleanVecOptional, boolean_vec)

#define DVEC_NAME PdfNameVec
#define DVEC_LOWERCASE_NAME pdf_name_vec
#define DVEC_TYPE PdfName
#include "arena/dvec_decl.h"

PDF_DECL_ARRAY_FIELD(PdfNameVec, name_vec)
PDF_DECL_AS_ARRAY_FIELD(PdfNameVec, name_vec)
PDF_DECL_OPTIONAL_FIELD(PdfNameVec*, PdfNameVecOptional, name_vec)
PDF_DECL_OPTIONAL_FIELD(PdfNameVec*, PdfAsNameVecOptional, as_name_vec)

typedef struct {
    enum { PDF_NUMBER_TYPE_INTEGER, PDF_NUMBER_TYPE_REAL } type;

    union {
        PdfInteger integer;
        PdfReal real;
    } value;
} PdfNumber;

Error* pdf_deserde_number(
    const PdfObject* object,
    PdfNumber* target_ptr,
    PdfResolver* resolver
);

Error* pdf_deserde_num_as_real(
    const PdfObject* object,
    PdfReal* target_ptr,
    PdfResolver* resolver
);

PDF_DECL_FIELD(PdfNumber, number)
PDF_DECL_FIELD(PdfReal, num_as_real)
PDF_DECL_OPTIONAL_FIELD(PdfNumber, PdfNumberOptional, number)
PDF_DECL_OPTIONAL_FIELD(PdfReal, PdfNumAsRealOptional, num_as_real)

#define DVEC_NAME PdfNumberVec
#define DVEC_LOWERCASE_NAME pdf_number_vec
#define DVEC_TYPE PdfNumber
#include "arena/dvec_decl.h"

PDF_DECL_ARRAY_FIELD(PdfNumberVec, number_vec)
PDF_DECL_FIXED_ARRAY_FIELD(PdfNumber, number)
PDF_DECL_OPTIONAL_FIELD(PdfNumberVec*, PdfNumberVecOptional, number_vec)

PdfReal pdf_number_as_real(PdfNumber number);
PdfObject pdf_number_as_object(PdfNumber number);
int pdf_number_cmp(PdfNumber lhs, PdfNumber rhs);

typedef struct {
    PdfNumber lower_left_x;
    PdfNumber lower_left_y;
    PdfNumber upper_right_x;
    PdfNumber upper_right_y;
} PdfRectangle;

Error* pdf_deserde_rectangle(
    const PdfObject* object,
    PdfRectangle* target_ptr,
    PdfResolver* resolver
);

PDF_DECL_FIELD(PdfRectangle, rectangle)
PDF_DECL_OPTIONAL_FIELD(PdfRectangle, PdfRectangleOptional, rectangle)

Error* pdf_deserde_geom_vec2(
    const PdfObject* object,
    GeomVec2* target_ptr,
    PdfResolver* resolver
);

PDF_DECL_FIELD(GeomVec2, geom_vec2)
PDF_DECL_OPTIONAL_FIELD(GeomVec2, PdfGeomVec2Optional, geom_vec2)

Error* pdf_deserde_geom_vec3(
    const PdfObject* object,
    GeomVec3* target_ptr,
    PdfResolver* resolver
);

PDF_DECL_FIELD(GeomVec3, geom_vec3)
PDF_DECL_OPTIONAL_FIELD(GeomVec3, PdfGeomVec3Optional, geom_vec3)

Error* pdf_deserde_pdf_mat(
    const PdfObject* object,
    GeomMat3* target_ptr,
    PdfResolver* resolver
);

Error* pdf_deserde_geom_mat3(
    const PdfObject* object,
    GeomMat3* target_ptr,
    PdfResolver* resolver
);

PDF_DECL_FIELD(GeomMat3, geom_mat3)
PDF_DECL_FIELD(GeomMat3, pdf_mat)
PDF_DECL_OPTIONAL_FIELD(GeomMat3, PdfGeomMat3Optional, geom_mat3)
PDF_DECL_OPTIONAL_FIELD(GeomMat3, PdfGeomPdfMatOptional, pdf_mat)
