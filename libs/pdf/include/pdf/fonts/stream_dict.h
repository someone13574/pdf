#pragma once

#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"

typedef struct {
    PdfIntegerOptional length1;
    PdfIntegerOptional length2;
    PdfIntegerOptional length3;
    PdfNameOptional subtype;
    PdfStreamOptional metadata;
} PdfFontStreamDict;

PdfError* pdf_deserialize_font_stream_dict(
    const PdfObject* object,
    PdfFontStreamDict* target_ptr,
    PdfResolver* resolver
);
