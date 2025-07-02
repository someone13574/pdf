#include "pdf.h"

#include <stdint.h>
#include <stdlib.h>

#include "ctx.h"
#include "log.h"
#include "parse.h"
#include "result.h"

struct PdfDocument {
    PdfCtx* ctx;
    uint8_t version;
    size_t startxref;
};

PdfDocument* pdf_document_new(char* buffer, size_t buffer_size) {
    // Allocate
    PdfDocument* doc = malloc(sizeof(PdfDocument));
    LOG_ASSERT(doc, "Failed to allocate document");
    doc->ctx = pdf_ctx_new(buffer, buffer_size);

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

    return doc;
}

void pdf_document_free(PdfDocument* doc) {
    if (doc) {
        pdf_ctx_free(doc->ctx);
        doc->ctx = NULL;

        free(doc);
    }
}
