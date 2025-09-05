#pragma once

#include "arena/arena.h"
#include "pdf/content_op.h"
#include "pdf/object.h"
#include "pdf_error/error.h"

typedef struct {
    PdfContentOpVec* operations;
} PdfContentStream;

PdfError* pdf_deserialize_content_stream(
    const PdfStream* stream,
    Arena* arena,
    PdfContentStream* deserialized
);
