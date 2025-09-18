#pragma once

#include <stdint.h>

#include "arena/arena.h"
#include "pdf_error/error.h"

typedef struct {
    size_t object_id;
    size_t generation;
} PdfIndirectRef;

typedef struct PdfResolver PdfResolver;
typedef struct PdfObject PdfObject;

typedef struct {
    bool present;
    bool unwrap_indirect_objs;
    PdfResolver* resolver;
} PdfOptionalResolver;

PdfError* pdf_resolver_new(
    Arena* arena,
    const uint8_t* buffer,
    size_t buffer_size,
    PdfResolver** resolver
);

PdfOptionalResolver pdf_op_resolver_some(PdfResolver* resolver);
PdfOptionalResolver pdf_op_resolver_none(bool unwrap_indirect_objs);
bool pdf_op_resolver_valid(PdfOptionalResolver resolver);

Arena* pdf_resolver_arena(PdfResolver* resolver);
PdfError*
pdf_resolve_ref(PdfResolver* resolver, PdfIndirectRef ref, PdfObject* resolved);

PdfError* pdf_resolve_object(
    PdfOptionalResolver resolver,
    const PdfObject* object,
    PdfObject* resolved
);
