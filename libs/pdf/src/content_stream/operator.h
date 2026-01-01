#pragma once

#include "../ctx.h"
#include "err/error.h"
#include "pdf/content_stream/operator.h"

Error* pdf_parse_operator(PdfCtx* ctx, PdfOperator* operator);
