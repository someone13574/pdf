#pragma once

#include "arena/arena.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"

typedef struct {
    PdfName type;
    PdfName subtype;
    PdfName base_font;

    PdfObject* raw_dict;
} PdfFont;

PdfError* pdf_deserialize_font(
    PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    PdfFont* deserialized
);
