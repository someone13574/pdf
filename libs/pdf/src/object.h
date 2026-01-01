#pragma once

#include "arena/arena.h"
#include "ctx.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"
#include "postscript/object.h"

PdfError* pdf_parse_object(
    PdfResolver* resolver,
    PdfObject* object_out,
    bool in_indirect_obj
);

PdfError*
pdf_parse_operand_object(Arena* arena, PdfCtx* ctx, PdfObject* object);

/// Converts a pdf object to a postscript object, if possible
PdfError*
pdf_object_into_postscript(const PdfObject* object, PSObject* out);

/// Converts a postscript object to a pdf object, if possible
PdfError* pdf_object_from_postscript(PSObject object, PdfObject* out);

char* pdf_string_as_cstr(PdfString pdf_string, Arena* arena);
