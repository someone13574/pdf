#pragma once

#include "../ctx.h"
#include "pdf/content_stream/operator.h"
#include "pdf_error/error.h"

PdfError* pdf_parse_operator(PdfCtx* ctx, PdfOperator* operator);
