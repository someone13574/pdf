#include "ctx.h"

#include <ctype.h>
#include <iso646.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena/arena.h"
#include "logger/log.h"
#include "pdf_error/error.h"

struct PdfCtx {
    const uint8_t* buffer;
    size_t buffer_len;
    size_t offset;
};

PdfCtx* pdf_ctx_new(Arena* arena, const uint8_t* buffer, size_t buffer_size) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(buffer, "Invalid buffer");
    RELEASE_ASSERT(buffer_size > 0, "Empty buffer");

    PdfCtx* ctx = arena_alloc(arena, sizeof(PdfCtx));
    RELEASE_ASSERT(ctx, "Allocation failed");

    ctx->buffer = buffer;
    ctx->buffer_len = buffer_size;
    ctx->offset = 0;

    return ctx;
}

size_t pdf_ctx_buffer_len(const PdfCtx* ctx) {
    RELEASE_ASSERT(ctx);
    return ctx->buffer_len;
}

size_t pdf_ctx_offset(const PdfCtx* ctx) {
    RELEASE_ASSERT(ctx);
    return ctx->offset;
}

PdfError* pdf_ctx_seek(PdfCtx* ctx, size_t offset) {
    RELEASE_ASSERT(ctx);
    LOG_DIAG(TRACE, CTX, "Seeking offset %zu", offset);

    if (offset > ctx->buffer_len) {
        return PDF_ERROR(PDF_ERR_CTX_EOF, "Sought past pdf end-of-file");
    }

    ctx->offset = offset;
    return NULL;
}

PdfError* pdf_ctx_shift(PdfCtx* ctx, int64_t relative_offset) {
    RELEASE_ASSERT(ctx);
    LOG_DIAG(TRACE, CTX, "Shifting offset by %ld", relative_offset);

    if (relative_offset < 0) {
        if (ctx->offset < (size_t)(-relative_offset)) {
            LOG_DIAG(
                TRACE,
                CTX,
                "New offset is out of bounds. Restoring offset to %zu",
                ctx->offset
            );
            return PDF_ERROR(
                PDF_ERR_CTX_EOF,
                "Relative seek past pdf start-of-file"
            );
        }

        ctx->offset -= (size_t)(-relative_offset);
    } else {
        size_t new_offset = ctx->offset + (size_t)relative_offset;
        if (new_offset > ctx->buffer_len) {
            LOG_DIAG(
                TRACE,
                CTX,
                "New offset is out of bounds. Restoring offset to %zu",
                ctx->offset
            );
            return PDF_ERROR(
                PDF_ERR_CTX_EOF,
                "Relative seek past pdf end-of-file"
            );
        }

        ctx->offset = new_offset;
    }

    LOG_DIAG(TRACE, CTX, "New ctx offset is %zu", ctx->offset);
    return NULL;
}

PdfError* pdf_ctx_peek_and_advance(PdfCtx* ctx, uint8_t* out) {
    RELEASE_ASSERT(ctx);

    if (out) {
        PDF_PROPAGATE(pdf_ctx_peek(ctx, out));
    }

    return pdf_ctx_seek(ctx, ctx->offset + 1);
}

PdfError* pdf_ctx_peek(const PdfCtx* ctx, uint8_t* out) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(out);

    if (ctx->offset >= ctx->buffer_len) {
        return PDF_ERROR(PDF_ERR_CTX_EOF, "Cannot peek end-of-file");
    }

    *out = ctx->buffer[ctx->offset];
    LOG_DIAG(TRACE, CTX, "Ctx char at offset %zu: '%c'", ctx->offset, *out);

    return NULL;
}

PdfError* pdf_ctx_peek_next(PdfCtx* ctx, uint8_t* out) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(out);

    size_t offset = ctx->offset;
    PDF_PROPAGATE(pdf_ctx_peek_and_advance(ctx, NULL));

    PdfError* peek_error = (pdf_ctx_peek(ctx, out));
    pdf_error_free_is_ok(pdf_ctx_seek(ctx, offset));

    if (peek_error) {
        return peek_error;
    }

    return NULL;
}

PdfError* pdf_ctx_expect(PdfCtx* ctx, const char* text) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(text);

    LOG_DIAG(DEBUG, CTX, "Expecting text \"%s\"", text);

    size_t restore_offset = ctx->offset;
    const char* offset_text = text;
    while (*offset_text != 0) {
        uint8_t peeked;
        PdfError* peek_error = pdf_ctx_peek(ctx, &peeked);
        if (peek_error) {
            pdf_error_free_is_ok(pdf_ctx_seek(ctx, restore_offset));
            return peek_error;
        }

        if ((uint8_t)*offset_text != peeked) {
            pdf_error_free_is_ok(pdf_ctx_seek(ctx, restore_offset));
            return PDF_ERROR(
                PDF_ERR_CTX_EXPECT,
                "Unexpected character `%c` at %zu in stream (expected `%c` for `%s`)",
                peeked,
                ctx->offset,
                *offset_text,
                offset_text
            );
        }

        PdfError* next_error = pdf_ctx_peek_and_advance(ctx, NULL);
        if (next_error) {
            pdf_error_free_is_ok(pdf_ctx_seek(ctx, restore_offset));
            return next_error;
        }

        offset_text++;
    }

    return NULL;
}

PdfError* pdf_ctx_require_byte_type(
    const PdfCtx* ctx,
    bool permit_eof,
    bool (*eval)(uint8_t)
) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(eval);

    LOG_DIAG(TRACE, CTX, "Expecting character type at offset %zu", ctx->offset);

    uint8_t peeked;
    PdfError* peek_error = (pdf_ctx_peek(ctx, &peeked));
    if (peek_error) {
        if (permit_eof) {
            pdf_error_free(peek_error);
            return NULL;
        }

        return peek_error;
    }

    if (!eval(peeked)) {
        return PDF_ERROR(
            PDF_ERR_CTX_EXPECT,
            "Character type wasn't the expected type"
        );
    }

    return NULL;
}

PdfError* pdf_ctx_backscan(PdfCtx* ctx, const char* text, size_t limit) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(text);

    LOG_DIAG(
        DEBUG,
        CTX,
        "Backscanning for text \"%s\" with byte limit %zu (0=none)",
        text,
        limit
    );

    size_t offset = ctx->offset;
    size_t restore_offset = ctx->offset;
    size_t count = 0;

    while (!pdf_error_free_is_ok(pdf_ctx_expect(ctx, text))) {
        PdfError* seek_error = pdf_ctx_shift(ctx, -1);
        if (seek_error) {
            pdf_error_free_is_ok(pdf_ctx_seek(ctx, restore_offset));
            return seek_error;
        }

        if (limit != 0 && ++count > limit) {
            pdf_error_free_is_ok(pdf_ctx_seek(ctx, restore_offset));
            return PDF_ERROR(
                PDF_ERR_CTX_SCAN_LIMIT,
                "Didn't find expected text within scan limit of %zu characters",
                limit
            );
        }

        offset = ctx->offset;
    }

    LOG_DIAG(TRACE, CTX, "Backscanned to %zu", offset);
    pdf_error_free_is_ok(pdf_ctx_seek(ctx, offset));
    return NULL;
}

// The CARRIAGE RETURN (0Dh) and LINE FEED (0Ah) characters, also called newline
// characters, shall be treated as end-of-line (EOL) markers. The combination of
// a CARRIAGE RETURN followed immediately by a LINE FEED shall be treated as one
// EOL marker.
PdfError* pdf_ctx_seek_line_start(PdfCtx* ctx) {
    RELEASE_ASSERT(ctx);

    LOG_DIAG(DEBUG, CTX, "Finding line start");

    size_t restore_offset = ctx->offset;
    if (restore_offset == ctx->buffer_len) {
        PDF_PROPAGATE(pdf_ctx_shift(ctx, -1));
    }

    uint8_t peeked;
    uint8_t prev_byte;
    PDF_PROPAGATE(pdf_ctx_peek(ctx, &peeked));

    do {
        PdfError* seek_error = pdf_ctx_shift(ctx, -1);

        if (seek_error && pdf_error_code(seek_error) == PDF_ERR_CTX_EOF) {
            // We are at the start of the file
            ctx->offset = 0;
            pdf_error_free(seek_error);
            return NULL;
        }

        if (seek_error) {
            pdf_error_free_is_ok(pdf_ctx_seek(ctx, restore_offset));
            return seek_error;
        }

        prev_byte = peeked;
        PdfError* peek_error = pdf_ctx_peek(ctx, &peeked);
        if (peek_error) {
            pdf_error_free_is_ok(pdf_ctx_seek(ctx, restore_offset));
            return peek_error;
        }
    } while ((peeked != '\n' && peeked != '\r')
             || (prev_byte == '\n' && peeked == '\r'));

    PdfError* advance_error = pdf_ctx_peek_and_advance(ctx, NULL);
    if (advance_error) {
        pdf_error_free_is_ok(pdf_ctx_seek(ctx, restore_offset));
        return advance_error;
    }

    return NULL;
}

PdfError* pdf_ctx_seek_next_line(PdfCtx* ctx) {
    RELEASE_ASSERT(ctx);

    LOG_DIAG(DEBUG, CTX, "Finding next line");

    size_t restore_offset = ctx->offset;

    uint8_t peeked;
    do {
        PdfError* error = pdf_ctx_peek_and_advance(ctx, &peeked);
        if (error) {
            pdf_error_free_is_ok(pdf_ctx_seek(ctx, restore_offset));
            return error;
        }
    } while (peeked != '\n' && peeked != '\r');

    size_t offset = ctx->offset;
    if (peeked == '\r' && ctx->offset != ctx->buffer_len) {
        PdfError* error = pdf_ctx_peek_and_advance(ctx, &peeked);
        if (error) {
            pdf_error_free_is_ok(pdf_ctx_seek(ctx, restore_offset));
            return error;
        }

        if (peeked != '\n') {
            pdf_error_free_is_ok(pdf_ctx_seek(ctx, offset));
        }
    }

    return NULL;
}

PdfError* pdf_ctx_consume_whitespace(PdfCtx* ctx) {
    RELEASE_ASSERT(ctx);

    LOG_DIAG(DEBUG, CTX, "Consuming whitespace");

    uint8_t peeked = ' ';
    while (pdf_error_free_is_ok(pdf_ctx_peek(ctx, &peeked))
           && is_pdf_whitespace(peeked)) {
        pdf_error_free_is_ok(pdf_ctx_peek_and_advance(ctx, NULL));
    }

    return NULL;
}

PdfError* pdf_ctx_consume_regular(PdfCtx* ctx) {
    RELEASE_ASSERT(ctx);

    LOG_DIAG(DEBUG, CTX, "Consuming regular");

    uint8_t peeked = ' ';
    while (pdf_error_free_is_ok(pdf_ctx_peek(ctx, &peeked))
           && is_pdf_regular(peeked)) {
        pdf_error_free_is_ok(pdf_ctx_peek_and_advance(ctx, NULL));
    }

    return NULL;
}

const uint8_t* pdf_ctx_get_raw(const PdfCtx* ctx) {
    RELEASE_ASSERT(ctx);
    return ctx->buffer;
}

PdfError* pdf_ctx_parse_int(
    PdfCtx* ctx,
    uint32_t* expected_length,
    uint64_t* value,
    uint32_t* actual_length
) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(value);

    LOG_DIAG(DEBUG, CTX, "Parsing int at %zu", ctx->offset);

    size_t start_offset = ctx->offset;

    uint8_t peeked;
    PdfError* error;

    uint64_t acc = 0;
    uint32_t processed_length = 0;

    while (!(error = pdf_ctx_peek_and_advance(ctx, &peeked))
           && isdigit(peeked)) {
        uint64_t digit = (uint64_t)(peeked - '0');
        processed_length++;

        if (acc <= (UINT64_MAX - digit) / 10) {
            acc *= 10;
            acc += digit;
        } else {
            acc = UINT32_MAX;
        }
    }

    if (expected_length && processed_length != *expected_length) {
        pdf_error_free_is_ok(pdf_ctx_seek(ctx, start_offset));
        if (!error) {
            return PDF_ERROR(
                PDF_ERR_CTX_EXPECT,
                "Parsed integer was not the expected length"
            );
        } else {
            return error;
        }
    }

    if (actual_length) {
        *actual_length = processed_length;
    }

    pdf_error_free_is_ok(pdf_ctx_seek(ctx, start_offset + processed_length));
    *value = acc;

    LOG_DIAG(TRACE, CTX, "Parsed int %lu. Length is %u", acc, processed_length);
    return NULL;
}

bool is_pdf_whitespace(uint8_t c) {
    return c == '\0' || c == '\t' || c == '\n' || c == '\f' || c == '\r'
        || c == ' ';
}

bool is_pdf_delimiter(uint8_t c) {
    return c == '(' || c == ')' || c == '<' || c == '>' || c == '[' || c == ']'
        || c == '{' || c == '}' || c == '/' || c == '%';
}

bool is_pdf_regular(uint8_t c) {
    return !is_pdf_whitespace(c) && !is_pdf_delimiter(c);
}

bool is_pdf_non_regular(uint8_t c) {
    return !is_pdf_regular(c);
}

#ifdef TEST
#include "test/test.h"

TEST_FUNC(test_ctx_expect_and_peek) {
    uint8_t buffer[] = "testing";
    Arena* arena = arena_new(128);

    PdfCtx* ctx =
        pdf_ctx_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    TEST_ASSERT(ctx);

    // Check peek
    uint8_t peeked;
    TEST_PDF_REQUIRE(pdf_ctx_peek(ctx, &peeked));
    TEST_ASSERT_EQ((uint8_t)'t', peeked);

    // Check next
    TEST_PDF_REQUIRE(pdf_ctx_peek_and_advance(ctx, &peeked));
    TEST_ASSERT_EQ((uint8_t)'t', peeked);

    // Check offset after partial match and invalid peek
    TEST_PDF_REQUIRE(pdf_ctx_expect(ctx, "est"));
    TEST_PDF_REQUIRE(pdf_ctx_expect(ctx, "ing"));
    TEST_PDF_REQUIRE_ERR(pdf_ctx_peek(ctx, &peeked), PDF_ERR_CTX_EOF);

    // Check offset restore on failure
    TEST_PDF_REQUIRE(pdf_ctx_seek(ctx, 0));
    TEST_PDF_REQUIRE_ERR(pdf_ctx_expect(ctx, "hi"), PDF_ERR_CTX_EXPECT);
    TEST_PDF_REQUIRE(pdf_ctx_expect(ctx, "testing"));

    // Check EOF
    TEST_PDF_REQUIRE(pdf_ctx_seek(ctx, 0));
    TEST_PDF_REQUIRE_ERR(pdf_ctx_expect(ctx, "testing!"), PDF_ERR_CTX_EOF);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(text_ctx_require_char_type) {
    uint8_t buffer[] = "the quick brown fox\t jumped( over the lazy dog";
    Arena* arena = arena_new(128);

    PdfCtx* ctx =
        pdf_ctx_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    TEST_ASSERT(ctx);

    TEST_PDF_REQUIRE(pdf_ctx_seek(ctx, 19));
    TEST_PDF_REQUIRE(pdf_ctx_require_byte_type(ctx, false, &is_pdf_whitespace));

    TEST_PDF_REQUIRE(pdf_ctx_seek(ctx, 27));
    TEST_PDF_REQUIRE(pdf_ctx_require_byte_type(ctx, false, &is_pdf_delimiter));

    TEST_PDF_REQUIRE(pdf_ctx_seek(ctx, 6));
    TEST_PDF_REQUIRE(pdf_ctx_require_byte_type(ctx, false, &is_pdf_regular));

    TEST_PDF_REQUIRE(pdf_ctx_seek(ctx, 6));
    TEST_PDF_REQUIRE_ERR(
        pdf_ctx_require_byte_type(ctx, false, &is_pdf_whitespace),
        PDF_ERR_CTX_EXPECT
    );

    TEST_PDF_REQUIRE(pdf_ctx_seek(ctx, 46));
    TEST_PDF_REQUIRE_ERR(
        pdf_ctx_require_byte_type(ctx, false, &is_pdf_whitespace),
        PDF_ERR_CTX_EOF
    );

    TEST_PDF_REQUIRE(pdf_ctx_seek(ctx, 46));
    TEST_PDF_REQUIRE(pdf_ctx_require_byte_type(ctx, true, &is_pdf_whitespace));

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ctx_backscan) {
    uint8_t buffer[] = "the quick brown fox jumped over the lazy dog";
    Arena* arena = arena_new(128);

    PdfCtx* ctx =
        pdf_ctx_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    TEST_ASSERT(ctx);

    TEST_PDF_REQUIRE(pdf_ctx_seek(ctx, pdf_ctx_buffer_len(ctx)));

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ctx_backscan_missing) {
    uint8_t buffer[] = "the quick brown fox jumped over the lazy dog";
    Arena* arena = arena_new(128);

    PdfCtx* ctx =
        pdf_ctx_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    TEST_ASSERT(ctx);
    TEST_PDF_REQUIRE(pdf_ctx_seek(ctx, pdf_ctx_buffer_len(ctx)));

    TEST_PDF_REQUIRE_ERR(pdf_ctx_backscan(ctx, "cat", 0), PDF_ERR_CTX_EOF);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ctx_backscan_limit) {
    uint8_t buffer[] = "the quick brown fox jumped over the lazy dog";
    Arena* arena = arena_new(128);

    PdfCtx* ctx =
        pdf_ctx_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    TEST_ASSERT(ctx);
    TEST_PDF_REQUIRE(pdf_ctx_seek(ctx, pdf_ctx_buffer_len(ctx)));

    TEST_PDF_REQUIRE(pdf_ctx_backscan(ctx, "the", 12));
    TEST_ASSERT_EQ((size_t)32, pdf_ctx_offset(ctx));

    TEST_PDF_REQUIRE_ERR(
        pdf_ctx_backscan(ctx, "fox", 15),
        PDF_ERR_CTX_SCAN_LIMIT
    );
    TEST_ASSERT_EQ((size_t)32, pdf_ctx_offset(ctx));

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ctx_seek_line_start) {
    uint8_t buffer[] = "line1\nline2\rline3\r\nline4\nline5";
    Arena* arena = arena_new(128);

    PdfCtx* ctx =
        pdf_ctx_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    TEST_ASSERT(ctx);

    TEST_PDF_REQUIRE(pdf_ctx_seek_line_start(ctx));
    TEST_ASSERT_EQ((size_t)0, pdf_ctx_offset(ctx));

    TEST_PDF_REQUIRE(pdf_ctx_seek(ctx, 3));
    TEST_PDF_REQUIRE(pdf_ctx_seek_line_start(ctx));
    TEST_ASSERT_EQ((size_t)0, pdf_ctx_offset(ctx));

    TEST_PDF_REQUIRE(pdf_ctx_seek(ctx, 6));
    TEST_PDF_REQUIRE(pdf_ctx_seek_line_start(ctx));
    TEST_ASSERT_EQ((size_t)6, pdf_ctx_offset(ctx));

    TEST_PDF_REQUIRE(pdf_ctx_seek(ctx, 11));
    TEST_PDF_REQUIRE(pdf_ctx_seek_line_start(ctx));
    TEST_ASSERT_EQ((size_t)6, pdf_ctx_offset(ctx));

    TEST_PDF_REQUIRE(pdf_ctx_seek(ctx, 18));
    TEST_PDF_REQUIRE(pdf_ctx_seek_line_start(ctx));
    TEST_ASSERT_EQ((size_t)12, pdf_ctx_offset(ctx));

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ctx_seek_next_line) {
    uint8_t buffer[] = "line1\nline2\rline3\r\nline4\nline5";
    Arena* arena = arena_new(128);

    PdfCtx* ctx =
        pdf_ctx_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    TEST_ASSERT(ctx);

    TEST_PDF_REQUIRE(pdf_ctx_seek_next_line(ctx));
    TEST_ASSERT_EQ((size_t)6, pdf_ctx_offset(ctx));

    TEST_PDF_REQUIRE(pdf_ctx_seek_next_line(ctx));
    TEST_ASSERT_EQ((size_t)12, pdf_ctx_offset(ctx));

    TEST_PDF_REQUIRE(pdf_ctx_seek(ctx, 11));
    TEST_PDF_REQUIRE(pdf_ctx_seek_next_line(ctx));
    TEST_ASSERT_EQ((size_t)12, pdf_ctx_offset(ctx));

    TEST_PDF_REQUIRE(pdf_ctx_seek(ctx, 18));
    TEST_PDF_REQUIRE(pdf_ctx_seek_next_line(ctx));
    TEST_ASSERT_EQ((size_t)19, pdf_ctx_offset(ctx));

    TEST_PDF_REQUIRE(pdf_ctx_seek(ctx, 27));
    TEST_PDF_REQUIRE_ERR(pdf_ctx_seek_next_line(ctx), PDF_ERR_CTX_EOF);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ctx_consume_whitespace) {
    uint8_t buffer[] = "there is a lot of whitespace             before this";
    Arena* arena = arena_new(128);

    PdfCtx* ctx =
        pdf_ctx_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    TEST_ASSERT(ctx);

    TEST_PDF_REQUIRE(pdf_ctx_seek(ctx, 28));
    TEST_PDF_REQUIRE(pdf_ctx_consume_whitespace(ctx));
    TEST_ASSERT_EQ((size_t)41, pdf_ctx_offset(ctx));

    TEST_PDF_REQUIRE(pdf_ctx_seek(ctx, 12));
    TEST_PDF_REQUIRE(pdf_ctx_consume_whitespace(ctx));
    TEST_ASSERT_EQ((size_t)12, pdf_ctx_offset(ctx));

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_ctx_parse_int) {
    uint8_t buffer[] = "John has +120 apples. I have 42";
    Arena* arena = arena_new(128);

    PdfCtx* ctx =
        pdf_ctx_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    TEST_ASSERT(ctx);

    uint64_t value;
    uint32_t expected_len = 3;
    uint32_t actual_len = 3;
    TEST_PDF_REQUIRE(pdf_ctx_seek(ctx, 10));
    TEST_PDF_REQUIRE(
        pdf_ctx_parse_int(ctx, &expected_len, &value, &actual_len)
    );
    TEST_ASSERT_EQ((uint64_t)120, value);
    TEST_ASSERT_EQ((uint32_t)3, actual_len);
    TEST_ASSERT_EQ((size_t)13, pdf_ctx_offset(ctx));

    expected_len = 2;
    TEST_PDF_REQUIRE(pdf_ctx_seek(ctx, 10));
    TEST_PDF_REQUIRE_ERR(
        pdf_ctx_parse_int(ctx, &expected_len, &value, NULL),
        PDF_ERR_CTX_EXPECT
    );

    expected_len = 2;
    TEST_PDF_REQUIRE(pdf_ctx_seek(ctx, 29));
    TEST_PDF_REQUIRE(pdf_ctx_parse_int(ctx, &expected_len, &value, NULL));
    TEST_ASSERT_EQ((uint64_t)42, value);

    expected_len = 3;
    TEST_PDF_REQUIRE(pdf_ctx_seek(ctx, 29));
    TEST_PDF_REQUIRE_ERR(
        pdf_ctx_parse_int(ctx, &expected_len, &value, NULL),
        PDF_ERR_CTX_EOF
    );

    TEST_PDF_REQUIRE(pdf_ctx_seek(ctx, 31));
    TEST_PDF_REQUIRE(pdf_ctx_parse_int(ctx, NULL, &value, &actual_len));
    TEST_ASSERT_EQ((uint32_t)0, actual_len);

    TEST_PDF_REQUIRE(pdf_ctx_seek(ctx, 5));
    TEST_PDF_REQUIRE(pdf_ctx_parse_int(ctx, NULL, &value, &actual_len));
    TEST_ASSERT_EQ((uint32_t)0, actual_len);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

#endif // TEST
