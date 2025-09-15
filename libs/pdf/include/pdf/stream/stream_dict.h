#pragma once

#include "arena/arena.h"
#include "pdf/deserde_types.h"
#include "pdf/object.h"
#include "pdf_error/error.h"

DESERIALIZABLE_ARRAY_TYPE(PdfNameArray)
DESERIALIZABLE_OPTIONAL_TYPE(PdfOpNameArray, PdfNameArray)

struct PdfStreamDict {
    PdfInteger length;
    PdfOpNameArray filter;

    // Additional entries in an embedded font stream dictionary
    PdfOpInteger length1;
    PdfOpInteger length2;
    PdfOpInteger length3;
    PdfOpName subtype;
    PdfOpStream metadata;

    const PdfObject* raw_dict;
};

PdfError* pdf_deserialize_stream_dict(
    const PdfObject* object,
    Arena* arena,
    PdfStreamDict* deserialized
);
