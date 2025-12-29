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

PdfError* pdf_resolver_new(
    Arena* arena,
    const uint8_t* buffer,
    size_t buffer_size,
    PdfResolver** resolver
);

Arena* pdf_resolver_arena(PdfResolver* resolver);
PdfError*
pdf_resolve_ref(PdfResolver* resolver, PdfIndirectRef ref, PdfObject* resolved);

PdfError* pdf_resolve_object(
    PdfResolver* resolver,
    const PdfObject* object,
    PdfObject* resolved,
    bool strip_objs
);
