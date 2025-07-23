#pragma once

#include "arena/arena.h"
#include "pdf/content_op.h"
#include "pdf/object.h"
#include "pdf/result.h"

typedef struct {
    PdfContentOpVec* operations;
} PdfContentStream;

PdfResult pdf_deserialize_content_stream(
    PdfStream* stream,
    Arena* arena,
    PdfContentStream* deserialized
);
