#pragma once

#include "arena.h"
#include "ctx.h"
#include "pdf_object.h"
#include "pdf_result.h"

PdfResult pdf_parse_object(
    Arena* arena,
    PdfCtx* ctx,
    PdfObject* object,
    bool in_indirect_obj
);
