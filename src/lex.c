#include "lex.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "test.h"

struct PdfCtx {
    char* buffer;
    size_t buffer_len;
    size_t offset;
};

PdfCtx* pdf_ctx_new(char* buffer, size_t buffer_size) {
    LOG_ASSERT(buffer, "Invalid buffer");
    LOG_ASSERT(buffer_size > 0, "Empty buffer");

    PdfCtx* ctx = malloc(sizeof(PdfCtx));
    LOG_ASSERT(ctx, "Allocation failed");

    ctx->buffer = buffer;
    ctx->buffer_len = buffer_size;
    ctx->offset = 0;

    return ctx;
}

void pdf_ctx_free(PdfCtx* ctx) {
    if (ctx) {
        free(ctx);
    }
}

size_t pdf_ctx_buffer_len(PdfCtx* ctx) {
    DBG_ASSERT(ctx);
    return ctx->buffer_len;
}

size_t pdf_ctx_offset(PdfCtx* ctx) {
    DBG_ASSERT(ctx);
    return ctx->offset;
}

PdfCtxResult pdf_ctx_seek(PdfCtx* ctx, size_t offset) {
    DBG_ASSERT(ctx);

    if (offset > ctx->buffer_len) {
        return PDF_CTX_ERR_EOF;
    }

    ctx->offset = offset;
    return PDF_CTX_OK;
}

PdfCtxResult pdf_ctx_shift(PdfCtx* ctx, ssize_t relative_offset) {
    ssize_t new_offset = (ssize_t)ctx->offset + relative_offset;
    if (new_offset < 0) {
        return PDF_CTX_ERR_EOF;
    }

    return pdf_ctx_seek(ctx, (size_t)new_offset);
}

PdfCtxResult pdf_ctx_next(PdfCtx* ctx, char* peeked) {
    if (peeked) {
        PdfCtxResult peek_ret = pdf_ctx_peek(ctx, peeked);
        if (peek_ret != PDF_CTX_OK) {
            return peek_ret;
        }
    }

    return pdf_ctx_seek(ctx, ctx->offset + 1);
}

PdfCtxResult pdf_ctx_peek(PdfCtx* ctx, char* peeked) {
    DBG_ASSERT(ctx);

    if (ctx->offset >= ctx->buffer_len) {
        return PDF_CTX_ERR_EOF;
    }

    if (peeked) {
        *peeked = ctx->buffer[ctx->offset];
    }

    return PDF_CTX_OK;
}

PdfCtxResult pdf_ctx_expect(PdfCtx* ctx, const char* text) {
    DBG_ASSERT(ctx);

    size_t restore_offset = ctx->offset;
    while (*text != 0) {
        char peeked;
        PdfCtxResult peek_ret = pdf_ctx_peek(ctx, &peeked);
        if (peek_ret != PDF_CTX_OK) {
            ctx->offset = restore_offset;
            return peek_ret;
        }

        if (*text != peeked) {
            ctx->offset = restore_offset;
            return PDF_CTX_ERR_EXPECT;
        }

        PdfCtxResult next_ret = pdf_ctx_next(ctx, NULL);
        if (next_ret != PDF_CTX_OK) {
            ctx->offset = restore_offset;
            return next_ret;
        }

        text++;
    }

    return PDF_CTX_OK;
}

TEST_FUNC(test_expect_and_peek) {
    char* buffer = "testing";
    PdfCtx* ctx = pdf_ctx_new(buffer, strlen(buffer));
    TEST_ASSERT(ctx, "Creation failed");

    // Check peek
    char peeked;
    TEST_ASSERT_EQ((PdfCtxResult)PDF_CTX_OK, pdf_ctx_peek(ctx, &peeked));
    TEST_ASSERT_EQ((char)'t', peeked);

    // Check next
    TEST_ASSERT_EQ((PdfCtxResult)PDF_CTX_OK, pdf_ctx_next(ctx, &peeked));
    TEST_ASSERT_EQ((char)'t', peeked);

    // Check offset after partial match and invalid peek
    TEST_ASSERT_EQ((PdfCtxResult)PDF_CTX_OK, pdf_ctx_expect(ctx, "est"));
    TEST_ASSERT_EQ((PdfCtxResult)PDF_CTX_OK, pdf_ctx_expect(ctx, "ing"));
    TEST_ASSERT_EQ((PdfCtxResult)PDF_CTX_ERR_EOF, pdf_ctx_peek(ctx, &peeked));

    // Check offset restore on failure
    TEST_ASSERT_EQ((PdfCtxResult)PDF_CTX_OK, pdf_ctx_seek(ctx, 0));
    TEST_ASSERT_EQ((PdfCtxResult)PDF_CTX_ERR_EXPECT, pdf_ctx_expect(ctx, "hi"));
    TEST_ASSERT_EQ((PdfCtxResult)PDF_CTX_OK, pdf_ctx_expect(ctx, "testing"));

    // Check EOF
    TEST_ASSERT_EQ((PdfCtxResult)PDF_CTX_OK, pdf_ctx_seek(ctx, 0));
    TEST_ASSERT_EQ(
        (PdfCtxResult)PDF_CTX_ERR_EOF,
        pdf_ctx_expect(ctx, "testing!")
    );

    pdf_ctx_free(ctx);

    return TEST_SUCCESS;
}
