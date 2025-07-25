#pragma once

#include "arena/arena.h"
#include "pdf/content_op.h"
#include "pdf/error.h"
#include "pdf/object.h"

typedef struct {
    PdfContentOpVec* operations;
} PdfContentStream;

PdfError* pdf_deserialize_content_stream(
    PdfStream* stream,
    Arena* arena,
    PdfContentStream* deserialized
);
