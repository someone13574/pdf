#pragma once

#include "arena.h"
#include "pdf_object.h"
#include "pdf_result.h"
#include "vec.h"

typedef struct {
    Vec* operations;
} PdfContentStream;

PdfResult pdf_deserialize_content_stream(
    PdfStream* stream,
    Arena* arena,
    PdfContentStream* deserialized
);
