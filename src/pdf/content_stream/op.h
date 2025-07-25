#pragma once

#include "operator.h"
#include "pdf/content_op.h"
#include "pdf/object.h"
#include "pdf_error/error.h"

PdfError* pdf_deserialize_content_op(
    PdfOperator op,
    PdfObjectVec* operands,
    Arena* arena,
    PdfContentOpVec* operation_queue
);
