#pragma once

#include "err/error.h"
#include "pdf/content_stream/operation.h"
#include "pdf/content_stream/operator.h"
#include "pdf/object.h"
#include "pdf/resolver.h"

Error* pdf_deser_content_op(
    PdfOperator op,
    const PdfObjectVec* operands,
    PdfResolver* resolver,
    PdfContentOpVec* operation_queue
);
