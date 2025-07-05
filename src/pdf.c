#include "pdf.h"

#include <stdint.h>
#include <stdlib.h>

#include "arena.h"
#include "ctx.h"
#include "log.h"
#include "parse.h"
#include "result.h"
#include "xref.h"

struct PdfDocument {
    PdfCtx* ctx;
    XRefTable* xref;
    uint8_t version;
    size_t startxref;

    Arena* arena;
};

PdfDocument* pdf_document_new(char* buffer, size_t buffer_size) {
    // Allocate
    Arena* arena = arena_new(1024);

    PdfDocument* doc = arena_alloc(arena, sizeof(PdfDocument));
    LOG_ASSERT(doc, "Failed to allocate document");
    doc->ctx = pdf_ctx_new(arena, buffer, buffer_size);
    doc->arena = arena;

    // Parse header
    LOG_ASSERT(
        pdf_parse_header(doc->ctx, &doc->version) == PDF_OK,
        "Invalid PDF header"
    );

    LOG_DEBUG_G("doc-info", "PDF Version 1.%hhu", doc->version);

    // Parse trailer
    LOG_ASSERT(
        pdf_parse_startxref(doc->ctx, &doc->startxref) == PDF_OK,
        "Invalid PDF trailer"
    );
    LOG_DEBUG_G("doc-info", "Startxref: %lu", doc->startxref);

    // Parse xref
    PdfResult result;
    XRefTable* xref = pdf_xref_new(arena, doc->ctx, doc->startxref, &result);
    LOG_ASSERT(xref, "Invalid xref");
    LOG_ASSERT(result == PDF_OK, "Failed to parse xref with code %d", result);
    doc->xref = xref;

    return doc;
}

void pdf_document_free(PdfDocument* doc) {
    if (doc) {
        arena_free(doc->arena);
    }
}
