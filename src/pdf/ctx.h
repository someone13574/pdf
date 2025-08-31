#pragma once

#include <stdint.h>
#include <stdio.h>

#include "arena/arena.h"
#include "pdf_error/error.h"

typedef struct PdfCtx PdfCtx;

// Creates a new context from a buffer. The buffer *must* be writable.
PdfCtx* pdf_ctx_new(Arena* arena, char* buffer, size_t buffer_size);

size_t pdf_ctx_buffer_len(PdfCtx* ctx);
size_t pdf_ctx_offset(PdfCtx* ctx);

PdfError* pdf_ctx_seek(PdfCtx* ctx, size_t offset);
PdfError* pdf_ctx_shift(PdfCtx* ctx, int64_t relative_offset);
PdfError* pdf_ctx_peek_and_advance(PdfCtx* ctx, char* out);

PdfError* pdf_ctx_peek(PdfCtx* ctx, char* out);
PdfError* pdf_ctx_peek_next(PdfCtx* ctx, char* out);
PdfError* pdf_ctx_expect(PdfCtx* ctx, const char* text);
PdfError*
pdf_ctx_require_char_type(PdfCtx* ctx, bool permit_eof, bool (*eval)(char));
PdfError* pdf_ctx_backscan(PdfCtx* ctx, const char* text, size_t limit);
PdfError* pdf_ctx_seek_line_start(PdfCtx* ctx);
PdfError* pdf_ctx_seek_next_line(PdfCtx* ctx);
PdfError* pdf_ctx_consume_whitespace(PdfCtx* ctx);

PdfError*
pdf_ctx_borrow_substr(PdfCtx* ctx, size_t offset, size_t length, char** substr);
PdfError* pdf_ctx_release_substr(PdfCtx* ctx);

PdfError* pdf_ctx_parse_int(
    PdfCtx* ctx,
    uint32_t* expected_length,
    uint64_t* value,
    uint32_t* actual_length
);

bool is_pdf_whitespace(char c);
bool is_pdf_delimiter(char c);
bool is_pdf_regular(char c);
bool is_pdf_non_regular(char c);
