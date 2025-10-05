#pragma once

#include "pdf/content_stream/operation.h"
#include "pdf/content_stream/operator.h"
#include "pdf/object.h"
#include "pdf_error/error.h"

PdfError* pdf_deserialize_content_op(
    PdfOperator op,
    const PdfObjectVec* operands,
    Arena* arena,
    PdfContentOpVec* operation_queue
);
