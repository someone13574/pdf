#pragma once

#include "pdf/object.h"
#include "pdf/resolver.h"

typedef struct {
    enum { PDF_NUMBER_TYPE_INTEGER, PDF_NUMBER_TYPE_REAL } type;

    union {
        PdfInteger integer;
        PdfReal real;
    } value;
} PdfNumber;

PdfError* pdf_deserialize_number(PdfObject* object, PdfNumber* deserialized);
PdfError* pdf_deserialize_number_wrapper(
    PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    void* deserialized
);

PdfReal pdf_number_as_real(PdfNumber number);

typedef struct {
    PdfNumber lower_left_x;
    PdfNumber lower_left_y;
    PdfNumber upper_right_x;
    PdfNumber upper_right_y;
} PdfRectangle;

PdfError*
pdf_deserialize_rectangle(PdfObject* object, PdfRectangle* deserialized);
PdfError* pdf_deserialize_rectangle_wrapper(
    PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    void* deserialized
);
