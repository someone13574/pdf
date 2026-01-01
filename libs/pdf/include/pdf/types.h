#pragma once

#include "pdf/deserde.h"
#include "pdf/object.h"
#include "pdf/resolver.h"

typedef int PdfUnimplemented;
typedef PdfObject PdfIgnored;

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

#define DVEC_NAME PdfNameVec
#define DVEC_LOWERCASE_NAME pdf_name_vec
#define DVEC_TYPE PdfName
#include "arena/dvec_decl.h"

PDF_DECL_OPTIONAL_FIELD(PdfNameVec*, PdfNameVecOptional, name_vec)

typedef struct {
    enum { PDF_NUMBER_TYPE_INTEGER, PDF_NUMBER_TYPE_REAL } type;

    union {
        PdfInteger integer;
        PdfReal real;
    } value;
} PdfNumber;

typedef struct {
    PdfNumber lower_left_x;
    PdfNumber lower_left_y;
    PdfNumber upper_right_x;
    PdfNumber upper_right_y;
} PdfRectangle;

Error* pdf_deserde_number(
    const PdfObject* object,
    PdfNumber* target_ptr,
    PdfResolver* resolver
);

PDF_DECL_FIELD(PdfReal, num_as_real)

PdfFieldDescriptor
pdf_num_as_real_optional_field(const char* key, PdfRealOptional* target_ptr);

Error* pdf_deserde_rectangle(
    const PdfObject* object,
    PdfRectangle* target_ptr,
    PdfResolver* resolver
);

Error* pdf_deserde_geom_vec3(
    const PdfObject* object,
    GeomVec3* target_ptr,
    PdfResolver* resolver
);

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
