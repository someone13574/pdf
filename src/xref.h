#pragma once

#include "ctx.h"
#include "result.h"

typedef struct XRefTable XRefTable;

XRefTable* pdf_xref_create(PdfCtx* ctx, size_t xrefstart, PdfResult* result);
void pdf_xref_free(XRefTable** xref);
