#pragma once

#include "arena/arena.h"
#include "ctx.h"
#include "err/error.h"
#include "pdf/object.h"
#include "pdf/resolver.h"

Error* pdf_parse_object(
    PdfResolver* resolver,
    PdfObject* object_out,
    bool in_indirect_obj
);

Error* pdf_parse_operand_object(Arena* arena, PdfCtx* ctx, PdfObject* object);

char* pdf_string_as_cstr(PdfString pdf_string, Arena* arena);
