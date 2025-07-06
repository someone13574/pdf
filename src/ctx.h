#pragma once

#include <stdio.h>

#include "arena.h"
#include "result.h"

typedef struct PdfCtx PdfCtx;

// Creates a new context from a buffer. The buffer *must* be writable.
PdfCtx* pdf_ctx_new(Arena* arena, char* buffer, size_t buffer_size);

size_t pdf_ctx_buffer_len(PdfCtx* ctx);
size_t pdf_ctx_offset(PdfCtx* ctx);

PdfResult pdf_ctx_seek(PdfCtx* ctx, size_t offset);
PdfResult pdf_ctx_shift(PdfCtx* ctx, ssize_t relative_offset);
PdfResult pdf_ctx_peek_and_advance(PdfCtx* ctx, char* out);

PdfResult pdf_ctx_peek(PdfCtx* ctx, char* out);
PdfResult pdf_ctx_peek_next(PdfCtx* ctx, char* out);
PdfResult pdf_ctx_expect(PdfCtx* ctx, const char* text);
PdfResult pdf_ctx_backscan(PdfCtx* ctx, const char* text, size_t limit);
PdfResult pdf_ctx_seek_line_start(PdfCtx* ctx);
PdfResult pdf_ctx_seek_next_line(PdfCtx* ctx);

PdfResult
pdf_ctx_borrow_substr(PdfCtx* ctx, size_t offset, size_t length, char** substr);
PdfResult pdf_ctx_release_substr(PdfCtx* ctx);

PdfResult pdf_ctx_parse_int(
    PdfCtx* ctx,
    size_t offset,
    size_t length,
    unsigned long long* out,
    long* read_length
);
