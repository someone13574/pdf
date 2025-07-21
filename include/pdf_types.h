#pragma once

#include "pdf_object.h"
#include "pdf_resolver.h"

typedef struct {
    enum { PDF_NUMBER_TYPE_INTEGER, PDF_NUMBER_TYPE_REAL } type;

    union {
        PdfInteger integer;
        PdfReal real;
    } value;
} PdfNumber;

PdfResult pdf_deserialize_number(PdfObject* object, PdfNumber* deserialized);
PdfResult pdf_deserialize_number_wrapper(
    PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    void* deserialized
);
