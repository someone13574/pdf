#pragma once

#include <stdio.h>

typedef struct PdfCtx PdfCtx;

typedef enum { PDF_CTX_OK, PDF_CTX_ERR_EOF, PDF_CTX_ERR_EXPECT } PdfCtxResult;

PdfCtx* pdf_ctx_new(char* buffer, size_t buffer_size);
void pdf_ctx_free(PdfCtx* ctx);

size_t pdf_ctx_buffer_len(PdfCtx* ctx);
size_t pdf_ctx_offset(PdfCtx* ctx);

PdfCtxResult pdf_ctx_seek(PdfCtx* ctx, size_t offset);
PdfCtxResult pdf_ctx_shift(PdfCtx* ctx, ssize_t relative_offset);
PdfCtxResult pdf_ctx_next(PdfCtx* ctx, char* out);

PdfCtxResult pdf_ctx_peek(PdfCtx* ctx, char* out);
PdfCtxResult pdf_ctx_expect(PdfCtx* ctx, const char* text);
