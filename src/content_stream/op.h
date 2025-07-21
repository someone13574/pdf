#pragma once

#include "operator.h"
#include "pdf_result.h"
#include "vec.h"

PdfResult pdf_deserialize_content_op(
    PdfOperator op,
    Vec* operands,
    Arena* arena,
    Vec* operation_queue
);
