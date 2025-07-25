#pragma once

#include "operator.h"
#include "pdf/content_stream.h"
#include "pdf/error.h"
#include "pdf/object.h"

PdfError* pdf_deserialize_content_op(
    PdfOperator op,
    PdfObjectVec* operands,
    Arena* arena,
    PdfContentOpVec* operation_queue
);
