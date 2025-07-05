#include "ctx.h"

#include <ctype.h>
#include <errno.h>
#include <iso646.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "log.h"
#include "result.h"

struct PdfCtx {
    char* buffer;
    size_t buffer_len;
    size_t offset;

    char borrow_term_replaced;
    size_t borrow_term_offset;
    char** borrowed_substr;
};

PdfCtx* pdf_ctx_new(Arena* arena, char* buffer, size_t buffer_size) {
    LOG_ASSERT(buffer, "Invalid buffer");
    LOG_ASSERT(buffer_size > 0, "Empty buffer");

    PdfCtx* ctx = arena_alloc(arena, sizeof(PdfCtx));
    LOG_ASSERT(ctx, "Allocation failed");

    ctx->buffer = buffer;
    ctx->buffer_len = buffer_size;
    ctx->offset = 0;
    ctx->borrow_term_replaced = '\0';
    ctx->borrow_term_offset = 0;
    ctx->borrowed_substr = NULL;

    return ctx;
}

size_t pdf_ctx_buffer_len(PdfCtx* ctx) {
    DBG_ASSERT(ctx);
    return ctx->buffer_len;
}

size_t pdf_ctx_offset(PdfCtx* ctx) {
    DBG_ASSERT(ctx);
    return ctx->offset;
}

PdfResult pdf_ctx_seek(PdfCtx* ctx, size_t offset) {
    DBG_ASSERT(ctx);
    LOG_TRACE_G("ctx", "Seeking offset %lu", offset);

    if (offset > ctx->buffer_len) {
        return PDF_CTX_ERR_EOF;
    }

    ctx->offset = offset;
    return PDF_OK;
}

PdfResult pdf_ctx_shift(PdfCtx* ctx, ssize_t relative_offset) {
    DBG_ASSERT(ctx);
    LOG_TRACE_G("ctx", "Shifting offset by %ld", relative_offset);

    if (relative_offset < 0) {
        if (ctx->offset < (size_t)(-relative_offset)) {
            LOG_TRACE_G(
                "ctx",
                "New offset is out of bounds. Restoring offset to %lu",
                ctx->offset
            );
            return PDF_CTX_ERR_EOF;
        }

        ctx->offset -= (size_t)(-relative_offset);
    } else {
        size_t new_offset = ctx->offset + (size_t)relative_offset;
        if (new_offset > ctx->buffer_len) {
            LOG_TRACE_G(
                "ctx",
                "New offset is out of bounds. Restoring offset to %lu",
                ctx->offset
            );
            return PDF_CTX_ERR_EOF;
        }

        ctx->offset = new_offset;
    }

    LOG_TRACE_G("ctx", "New ctx offset is %lu", ctx->offset);
    return PDF_OK;
}

PdfResult pdf_ctx_peek_and_advance(PdfCtx* ctx, char* peeked) {
    if (ctx->borrowed_substr) {
        return PDF_CTX_ERR_BORROWED;
    }

    if (peeked) {
        PDF_TRY(pdf_ctx_peek(ctx, peeked));
    }

    return pdf_ctx_seek(ctx, ctx->offset + 1);
}

PdfResult pdf_ctx_peek(PdfCtx* ctx, char* peeked) {
    DBG_ASSERT(ctx);
    if (ctx->borrowed_substr) {
        return PDF_CTX_ERR_BORROWED;
    }

    if (ctx->offset >= ctx->buffer_len) {
        return PDF_CTX_ERR_EOF;
    }

    if (peeked) {
        *peeked = ctx->buffer[ctx->offset];
        LOG_TRACE_G(
            "ctx",
            "Ctx char at offset %lu: '%c'",
            ctx->offset,
            *peeked
        );
    }

    return PDF_OK;
}

PdfResult pdf_ctx_expect(PdfCtx* ctx, const char* text) {
    DBG_ASSERT(ctx);
    LOG_DEBUG_G("ctx", "Expecting text \"%s\"", text);

    if (ctx->borrowed_substr) {
        return PDF_CTX_ERR_BORROWED;
    }

    size_t restore_offset = ctx->offset;
    while (*text != 0) {
        char peeked;
        PdfResult peek_ret = pdf_ctx_peek(ctx, &peeked);
        if (peek_ret != PDF_OK) {
            pdf_ctx_seek(ctx, restore_offset);
            return peek_ret;
        }

        if (*text != peeked) {
            pdf_ctx_seek(ctx, restore_offset);
            return PDF_CTX_ERR_EXPECT;
        }

        PdfResult next_ret = pdf_ctx_peek_and_advance(ctx, NULL);
        if (next_ret != PDF_OK) {
            pdf_ctx_seek(ctx, restore_offset);
            return next_ret;
        }

        text++;
    }

    return PDF_OK;
}

PdfResult pdf_ctx_backscan(PdfCtx* ctx, const char* text, size_t limit) {
    DBG_ASSERT(ctx);
    LOG_DEBUG_G(
        "ctx",
        "Backscanning for text \"%s\" with char limit %lu (0=none)",
        text,
        limit
    );

    if (ctx->borrowed_substr) {
        return PDF_CTX_ERR_BORROWED;
    }

    size_t offset = ctx->offset;
    size_t restore_offset = ctx->offset;
    size_t count = 0;

    while (pdf_ctx_expect(ctx, text) != PDF_OK) {
        PdfResult seek_result = pdf_ctx_shift(ctx, -1);
        if (seek_result != PDF_OK) {
            pdf_ctx_seek(ctx, restore_offset);
            return seek_result;
        }

        if (limit != 0 && ++count > limit) {
            pdf_ctx_seek(ctx, restore_offset);
            return PDF_CTX_ERR_SCAN_LIMIT;
        }

        offset = ctx->offset;
    }

    pdf_ctx_seek(ctx, offset);
    return PDF_OK;
}

// The CARRIAGE RETURN (0Dh) and LINE FEED (0Ah) characters, also called newline
// characters, shall be treated as end-of-line (EOL) markers. The combination of
// a CARRIAGE RETURN followed immediately by a LINE FEED shall be treated as one
// EOL marker.
PdfResult pdf_ctx_seek_line_start(PdfCtx* ctx) {
    DBG_ASSERT(ctx);
    LOG_DEBUG_G("ctx", "Finding line start");

    if (ctx->borrowed_substr) {
        return PDF_CTX_ERR_BORROWED;
    }

    char peeked;
    char prev_char;
    PDF_TRY(pdf_ctx_peek(ctx, &peeked));

    size_t restore_offset = ctx->offset;

    do {
        PdfResult seek_result = pdf_ctx_shift(ctx, -1);
        switch (seek_result) {
            case PDF_OK: {
                break;
            }
            case PDF_CTX_ERR_EOF: {
                ctx->offset = 0;
                return PDF_OK;
            }
            default: {
                pdf_ctx_seek(ctx, restore_offset);
                return seek_result;
            }
        }

        prev_char = peeked;
        PdfResult peek_result = pdf_ctx_peek(ctx, &peeked);
        if (peek_result != PDF_OK) {
            pdf_ctx_seek(ctx, restore_offset);
            return peek_result;
        }
    } while ((peeked != '\n' && peeked != '\r')
             || (prev_char == '\n' && peeked == '\r'));

    PdfResult advance_result = pdf_ctx_peek_and_advance(ctx, NULL);
    if (advance_result != PDF_OK) {
        pdf_ctx_seek(ctx, restore_offset);
        return advance_result;
    }

    return PDF_OK;
}

PdfResult pdf_ctx_seek_next_line(PdfCtx* ctx) {
    DBG_ASSERT(ctx);
    LOG_DEBUG_G("ctx", "Finding next line");

    if (ctx->borrowed_substr) {
        return PDF_CTX_ERR_BORROWED;
    }

    size_t restore_offset = ctx->offset;

    char peeked;
    do {
        PdfResult result = pdf_ctx_peek_and_advance(ctx, &peeked);
        if (result != PDF_OK) {
            pdf_ctx_seek(ctx, restore_offset);
            return result;
        }
    } while (peeked != '\n' && peeked != '\r');

    size_t offset = ctx->offset;
    if (peeked == '\r' && ctx->offset != ctx->buffer_len) {
        PdfResult result = pdf_ctx_peek_and_advance(ctx, &peeked);
        if (result != PDF_OK) {
            pdf_ctx_seek(ctx, restore_offset);
            return result;
        }

        if (peeked != '\n') {
            pdf_ctx_seek(ctx, offset);
        }
    }

    return PDF_OK;
}

PdfResult pdf_ctx_borrow_substr(
    PdfCtx* ctx,
    size_t offset,
    size_t length,
    char** substr
) {
    DBG_ASSERT(ctx);
    DBG_ASSERT(substr);
    LOG_ASSERT(length > 0, "Cannot borrow zero length substr");
    LOG_DEBUG_G(
        "ctx",
        "Borrowing substring starting at %lu with length %lu",
        offset,
        length
    );

    if (ctx->borrowed_substr) {
        return PDF_CTX_ERR_BORROWED;
    }

    ctx->borrow_term_offset = offset + length;
    if (ctx->borrow_term_offset >= ctx->buffer_len) {
        return PDF_CTX_ERR_EOF;
    }

    ctx->borrowed_substr = substr;
    *substr = ctx->buffer + offset;

    ctx->borrow_term_replaced = ctx->buffer[ctx->borrow_term_offset];
    ctx->buffer[ctx->borrow_term_offset] = '\0';
    LOG_TRACE_G("ctx", "Borrowed substr: \"%s\"", *substr);

    return PDF_OK;
}

PdfResult pdf_ctx_release_substr(PdfCtx* ctx) {
    DBG_ASSERT(ctx);
    LOG_DEBUG_G("ctx", "Releasing substr");

    if (!ctx->borrowed_substr) {
        return PDF_CTX_ERR_NOT_BORROWED;
    }

    ctx->buffer[ctx->borrow_term_offset] = ctx->borrow_term_replaced;
    ctx->borrow_term_offset = 0;
    ctx->borrow_term_replaced = '\0';
    *ctx->borrowed_substr = NULL;
    ctx->borrowed_substr = NULL;

    return PDF_OK;
}

PdfResult pdf_ctx_parse_int(
    PdfCtx* ctx,
    size_t offset,
    size_t length,
    unsigned long long* out,
    long* read_length
) {
    DBG_ASSERT(ctx);
    DBG_ASSERT(out);
    LOG_DEBUG_G(
        "ctx",
        "Parsing int starting at %lu with length upto %lu",
        offset,
        length
    );

    char* substr;
    PDF_TRY(pdf_ctx_borrow_substr(ctx, offset, length, &substr));

    if (substr[0] == '+' || isspace(substr[0])) {
        PDF_TRY(pdf_ctx_release_substr(ctx));
        return PDF_CTX_ERR_INVALID_NUMBER;
    }

    char* end;
    errno = 0;
    *out = strtoull(substr, &end, 10);

    if (substr == end || errno == ERANGE) {
        PDF_TRY(pdf_ctx_release_substr(ctx));
        return PDF_CTX_ERR_INVALID_NUMBER;
    }

    if (read_length) {
        *read_length = end - substr;
    }

    PDF_TRY(pdf_ctx_release_substr(ctx));
    LOG_DEBUG_G("ctx", "Parsed int %llu", *out);
    return PDF_OK;
}

#ifdef TEST
#include "test.h"

TEST_FUNC(test_ctx_expect_and_peek) {
    char buffer[] = "testing";
    Arena* arena = arena_new(128);

    PdfCtx* ctx = pdf_ctx_new(arena, buffer, strlen(buffer));
    TEST_ASSERT(ctx);

    // Check peek
    char peeked;
    TEST_ASSERT_EQ((PdfResult)PDF_OK, pdf_ctx_peek(ctx, &peeked));
    TEST_ASSERT_EQ((char)'t', peeked);

    // Check next
    TEST_ASSERT_EQ((PdfResult)PDF_OK, pdf_ctx_peek_and_advance(ctx, &peeked));
    TEST_ASSERT_EQ((char)'t', peeked);

    // Check offset after partial match and invalid peek
    TEST_ASSERT_EQ((PdfResult)PDF_OK, pdf_ctx_expect(ctx, "est"));
    TEST_ASSERT_EQ((PdfResult)PDF_OK, pdf_ctx_expect(ctx, "ing"));
    TEST_ASSERT_EQ((PdfResult)PDF_CTX_ERR_EOF, pdf_ctx_peek(ctx, &peeked));

    // Check offset restore on failure
    TEST_ASSERT_EQ((PdfResult)PDF_OK, pdf_ctx_seek(ctx, 0));
    TEST_ASSERT_EQ((PdfResult)PDF_CTX_ERR_EXPECT, pdf_ctx_expect(ctx, "hi"));
    TEST_ASSERT_EQ((PdfResult)PDF_OK, pdf_ctx_expect(ctx, "testing"));

    // Check EOF
    TEST_ASSERT_EQ((PdfResult)PDF_OK, pdf_ctx_seek(ctx, 0));
    TEST_ASSERT_EQ((PdfResult)PDF_CTX_ERR_EOF, pdf_ctx_expect(ctx, "testing!"));

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ctx_backscan) {
    char buffer[] = "the quick brown fox jumped over the lazy dog";
    Arena* arena = arena_new(128);

    PdfCtx* ctx = pdf_ctx_new(arena, buffer, strlen(buffer));
    TEST_ASSERT(ctx);
    TEST_ASSERT_EQ(
        (PdfResult)PDF_OK,
        pdf_ctx_seek(ctx, pdf_ctx_buffer_len(ctx))
    );

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ctx_backscan_missing) {
    char buffer[] = "the quick brown fox jumped over the lazy dog";
    Arena* arena = arena_new(128);

    PdfCtx* ctx = pdf_ctx_new(arena, buffer, strlen(buffer));
    TEST_ASSERT(ctx);
    TEST_ASSERT_EQ(
        (PdfResult)PDF_OK,
        pdf_ctx_seek(ctx, pdf_ctx_buffer_len(ctx))
    );

    TEST_ASSERT_EQ((PdfResult)PDF_CTX_ERR_EOF, pdf_ctx_backscan(ctx, "cat", 0));

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ctx_backscan_limit) {
    char buffer[] = "the quick brown fox jumped over the lazy dog";
    Arena* arena = arena_new(128);

    PdfCtx* ctx = pdf_ctx_new(arena, buffer, strlen(buffer));
    TEST_ASSERT(ctx);
    TEST_ASSERT_EQ(
        (PdfResult)PDF_OK,
        pdf_ctx_seek(ctx, pdf_ctx_buffer_len(ctx))
    );

    TEST_ASSERT_EQ((PdfResult)PDF_OK, pdf_ctx_backscan(ctx, "the", 12));
    TEST_ASSERT_EQ((size_t)32, pdf_ctx_offset(ctx));

    TEST_ASSERT_EQ(
        (PdfResult)PDF_CTX_ERR_SCAN_LIMIT,
        pdf_ctx_backscan(ctx, "fox", 15)
    );
    TEST_ASSERT_EQ((size_t)32, pdf_ctx_offset(ctx));

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ctx_seek_line_start) {
    char buffer[] = "line1\nline2\rline3\r\nline4\nline5";
    Arena* arena = arena_new(128);

    PdfCtx* ctx = pdf_ctx_new(arena, buffer, strlen(buffer));
    TEST_ASSERT(ctx);

    TEST_ASSERT_EQ((PdfResult)PDF_OK, pdf_ctx_seek_line_start(ctx));
    TEST_ASSERT_EQ((size_t)0, pdf_ctx_offset(ctx));

    TEST_ASSERT_EQ((PdfResult)PDF_OK, pdf_ctx_seek(ctx, 3));
    TEST_ASSERT_EQ((PdfResult)PDF_OK, pdf_ctx_seek_line_start(ctx));
    TEST_ASSERT_EQ((size_t)0, pdf_ctx_offset(ctx));

    TEST_ASSERT_EQ((PdfResult)PDF_OK, pdf_ctx_seek(ctx, 6));
    TEST_ASSERT_EQ((PdfResult)PDF_OK, pdf_ctx_seek_line_start(ctx));
    TEST_ASSERT_EQ((size_t)6, pdf_ctx_offset(ctx));

    TEST_ASSERT_EQ((PdfResult)PDF_OK, pdf_ctx_seek(ctx, 11));
    TEST_ASSERT_EQ((PdfResult)PDF_OK, pdf_ctx_seek_line_start(ctx));
    TEST_ASSERT_EQ((size_t)6, pdf_ctx_offset(ctx));

    TEST_ASSERT_EQ((PdfResult)PDF_OK, pdf_ctx_seek(ctx, 18));
    TEST_ASSERT_EQ((PdfResult)PDF_OK, pdf_ctx_seek_line_start(ctx));
    TEST_ASSERT_EQ((size_t)12, pdf_ctx_offset(ctx));

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ctx_seek_next_line) {
    char buffer[] = "line1\nline2\rline3\r\nline4\nline5";
    Arena* arena = arena_new(128);

    PdfCtx* ctx = pdf_ctx_new(arena, buffer, strlen(buffer));
    TEST_ASSERT(ctx);

    TEST_ASSERT_EQ((PdfResult)PDF_OK, pdf_ctx_seek_next_line(ctx));
    TEST_ASSERT_EQ((size_t)6, pdf_ctx_offset(ctx));

    TEST_ASSERT_EQ((PdfResult)PDF_OK, pdf_ctx_seek_next_line(ctx));
    TEST_ASSERT_EQ((size_t)12, pdf_ctx_offset(ctx));

    TEST_ASSERT_EQ((PdfResult)PDF_OK, pdf_ctx_seek(ctx, 11));
    TEST_ASSERT_EQ((PdfResult)PDF_OK, pdf_ctx_seek_next_line(ctx));
    TEST_ASSERT_EQ((size_t)12, pdf_ctx_offset(ctx));

    TEST_ASSERT_EQ((PdfResult)PDF_OK, pdf_ctx_seek(ctx, 18));
    TEST_ASSERT_EQ((PdfResult)PDF_OK, pdf_ctx_seek_next_line(ctx));
    TEST_ASSERT_EQ((size_t)19, pdf_ctx_offset(ctx));

    TEST_ASSERT_EQ((PdfResult)PDF_OK, pdf_ctx_seek(ctx, 27));
    TEST_ASSERT_EQ((PdfResult)PDF_CTX_ERR_EOF, pdf_ctx_seek_next_line(ctx));

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ctx_borrow_substr) {
    char buffer[] = "the quick brown fox jumped over the lazy dog";
    Arena* arena = arena_new(128);

    PdfCtx* ctx = pdf_ctx_new(arena, buffer, strlen(buffer));
    TEST_ASSERT(ctx);

    // Borrow
    char* substr;
    TEST_ASSERT_EQ(
        (PdfResult)PDF_OK,
        pdf_ctx_borrow_substr(ctx, 16, 3, &substr)
    );
    TEST_ASSERT_EQ("fox", substr);

    // Try double borrow
    char* substr2;
    TEST_ASSERT_EQ(
        (PdfResult)PDF_CTX_ERR_BORROWED,
        pdf_ctx_borrow_substr(ctx, 4, 5, &substr2)
    );

    // Free original borrow
    TEST_ASSERT_EQ((PdfResult)PDF_OK, pdf_ctx_release_substr(ctx));
    TEST_ASSERT(!substr);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ctx_parse_int) {
    char buffer[] = "John has +120 apples. I have 42.";
    Arena* arena = arena_new(128);

    PdfCtx* ctx = pdf_ctx_new(arena, buffer, strlen(buffer));
    TEST_ASSERT(ctx);

    unsigned long long out;
    long read_length;
    TEST_ASSERT_EQ(
        (PdfResult)PDF_OK,
        pdf_ctx_parse_int(ctx, 10, 4, &out, &read_length)
    );
    TEST_ASSERT_EQ((unsigned long long)120, out);
    TEST_ASSERT_EQ(3L, read_length);

    TEST_ASSERT_EQ(
        (PdfResult)PDF_CTX_ERR_INVALID_NUMBER,
        pdf_ctx_parse_int(ctx, 14, 2, &out, NULL)
    );

    TEST_ASSERT_EQ(
        (PdfResult)PDF_OK,
        pdf_ctx_parse_int(ctx, 29, 2, &out, NULL)
    );
    TEST_ASSERT_EQ((unsigned long long)42, out);

    TEST_ASSERT_EQ(
        (PdfResult)PDF_CTX_ERR_INVALID_NUMBER,
        pdf_ctx_parse_int(ctx, 9, 4, &out, NULL)
    );

    TEST_ASSERT_EQ(
        (PdfResult)PDF_CTX_ERR_INVALID_NUMBER,
        pdf_ctx_parse_int(ctx, 28, 3, &out, NULL)
    );

    arena_free(arena);
    return TEST_RESULT_PASS;
}

#endif // TEST
