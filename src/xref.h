#pragma once

#include "ctx.h"
#include "result.h"

typedef struct XRefTable XRefTable;

XRefTable*
pdf_xref_new(Arena* arena, PdfCtx* ctx, size_t xrefstart, PdfResult* result);
