#pragma once

#include "arena/arena.h"
#include "pdf/error.h"
#include "pdf/object.h"

typedef struct PdfResolver PdfResolver;

typedef struct {
    bool present;
    bool unwrap_indirect_objs;
    PdfResolver* resolver;
} PdfOptionalResolver;

PdfError* pdf_resolver_new(
    Arena* arena,
    char* buffer,
    size_t buffer_size,
    PdfResolver** resolver
);

PdfOptionalResolver pdf_op_resolver_some(PdfResolver* resolver);
PdfOptionalResolver pdf_op_resolver_none(bool unwrap_indirect_objs);
bool pdf_op_resolver_valid(PdfOptionalResolver resolver);

Arena* pdf_resolver_arena(PdfResolver* resolver);
PdfError*
pdf_resolve_ref(PdfResolver* resolver, PdfIndirectRef ref, PdfObject* resolved);
