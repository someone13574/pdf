#pragma once

#include "arena.h"
#include "ctx.h"
#include "pdf_object.h"
#include "pdf_result.h"

PdfObject* pdf_parse_object(
    Arena* arena,
    PdfCtx* ctx,
    PdfResult* result,
    bool in_direct_obj
);
