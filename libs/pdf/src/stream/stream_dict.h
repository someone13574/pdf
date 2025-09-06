#pragma once

#include "arena/arena.h"
#include "pdf/deserde_types.h"
#include "pdf/object.h"
#include "pdf_error/error.h"

DESERIALIZABLE_ARRAY_TYPE(PdfNameArray)
DESERIALIZABLE_OPTIONAL_TYPE(PdfOpNameArray, PdfNameArray)

typedef struct {
    PdfInteger length;
    PdfOpNameArray filter;

    const PdfObject* raw_dict;
} PdfStreamDict;

PdfError* pdf_deserialize_stream_dict(
    const PdfObject* object,
    Arena* arena,
    PdfStreamDict* deserialized
);
