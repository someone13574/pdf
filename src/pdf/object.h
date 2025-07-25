#pragma once

#include "arena/arena.h"
#include "ctx.h"
#include "pdf/object.h"
#include "pdf_error/error.h"

PdfError* pdf_parse_object(
    Arena* arena,
    PdfCtx* ctx,
    PdfObject* object,
    bool in_indirect_obj
);

PdfError*
pdf_parse_operand_object(Arena* arena, PdfCtx* ctx, PdfObject* object);
