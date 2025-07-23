#pragma once

#include "arena/arena.h"
#include "ctx.h"
#include "pdf/object.h"
#include "pdf/result.h"

PdfResult pdf_parse_object(
    Arena* arena,
    PdfCtx* ctx,
    PdfObject* object,
    bool in_indirect_obj
);

PdfResult
pdf_parse_operand_object(Arena* arena, PdfCtx* ctx, PdfObject* object);
