#pragma once

#include <stdint.h>
#include <stdio.h>

#include "arena/arena.h"
#include "pdf_error/error.h"

typedef struct PdfCtx PdfCtx;

// Creates a new context from a buffer. The buffer *must* be writable.
PdfCtx* pdf_ctx_new(Arena* arena, const uint8_t* buffer, size_t buffer_size);

size_t pdf_ctx_buffer_len(const PdfCtx* ctx);
size_t pdf_ctx_offset(const PdfCtx* ctx);

PdfError* pdf_ctx_seek(PdfCtx* ctx, size_t offset);
PdfError* pdf_ctx_shift(PdfCtx* ctx, int64_t relative_offset);
PdfError* pdf_ctx_peek_and_advance(PdfCtx* ctx, uint8_t* out);

PdfError* pdf_ctx_peek(const PdfCtx* ctx, uint8_t* out);
PdfError* pdf_ctx_peek_next(PdfCtx* ctx, uint8_t* out);
PdfError* pdf_ctx_expect(PdfCtx* ctx, const char* text);
PdfError* pdf_ctx_require_byte_type(
    const PdfCtx* ctx,
    bool permit_eof,
    bool (*eval)(uint8_t)
);
PdfError* pdf_ctx_backscan(PdfCtx* ctx, const char* text, size_t limit);
PdfError* pdf_ctx_seek_line_start(PdfCtx* ctx);
PdfError* pdf_ctx_seek_next_line(PdfCtx* ctx);
PdfError* pdf_ctx_consume_whitespace(PdfCtx* ctx);
PdfError* pdf_ctx_consume_regular(PdfCtx* ctx);

const uint8_t* pdf_ctx_get_raw(const PdfCtx* ctx);

PdfError* pdf_ctx_parse_int(
    PdfCtx* ctx,
    uint32_t* expected_length,
    uint64_t* value,
    uint32_t* actual_length
);

bool is_pdf_whitespace(uint8_t c);
bool is_pdf_delimiter(uint8_t c);
bool is_pdf_regular(uint8_t c);
bool is_pdf_non_regular(uint8_t c);
