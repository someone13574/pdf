#pragma once

#include <stdint.h>

#include "ctx.h"

PdfResult pdf_parse_header(PdfCtx* ctx, uint8_t* version);
PdfResult pdf_parse_startxref(PdfCtx* ctx, size_t* startxref);
