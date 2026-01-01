#pragma once

#include "err/error.h"
#include "pdf/object.h"
#include "pdf/resolver.h"

typedef struct {
    PdfIntegerOptional length1;
    PdfIntegerOptional length2;
    PdfIntegerOptional length3;
    PdfNameOptional subtype;
    PdfStreamOptional metadata;
} PdfFontStreamDict;

Error* pdf_deserde_font_stream_dict(
    const PdfObject* object,
    PdfFontStreamDict* target_ptr,
    PdfResolver* resolver
);
