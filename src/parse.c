#include "parse.h"

#include <stdint.h>
#include <stdlib.h>

#include "arena.h"
#include "ctx.h"
#include "log.h"
#include "result.h"

// The first line of a PDF file shall be a header consisting of the 5 characters
// %PDF– followed by a version number of the form 1.N, where N is a digit
// between 0 and 7.
PdfResult pdf_parse_header(PdfCtx* ctx, uint8_t* version) {
    DBG_ASSERT(version);

    char version_char;
    PDF_TRY(pdf_ctx_expect(ctx, "%PDF-1."));
    PDF_TRY(pdf_ctx_peek_and_advance(ctx, &version_char));

    if (version_char < '0' || version_char > '7') {
        return PDF_ERR_INVALID_VERSION;
    } else {
        *version = (uint8_t)(version_char - '0');
    }

    return PDF_OK;
}

// The last line of the file shall contain only the end-of-file marker, %%EOF.
// The two preceding lines shall contain, one per line and in order, the keyword
// startxref and the byte offset in the decoded stream from the beginning of the
// file to the beginning of the xref keyword in the last cross-reference
// section.
PdfResult pdf_parse_startxref(PdfCtx* ctx, size_t* startxref) {
    DBG_ASSERT(startxref);

    // Find EOF marker
    PDF_TRY(pdf_ctx_seek(ctx, pdf_ctx_buffer_len(ctx)));
    PDF_TRY(pdf_ctx_backscan(ctx, "%%EOF", 32));

    size_t eof_marker_offset = pdf_ctx_offset(ctx);
    PDF_TRY(pdf_ctx_seek_line_start(ctx));
    if (eof_marker_offset != pdf_ctx_offset(ctx)) {
        return PDF_ERR_INVALID_TRAILER;
    }

    // Get byte offset
    PDF_TRY(pdf_ctx_shift(ctx, -1));
    PDF_TRY(pdf_ctx_seek_line_start(ctx));
    size_t startxref_offset = pdf_ctx_offset(ctx);

    unsigned long long xref_offset;
    PDF_TRY(pdf_ctx_parse_int(
        ctx,
        startxref_offset,
        eof_marker_offset - startxref_offset,
        &xref_offset,
        NULL
    ));

    *startxref = (size_t)xref_offset;

    // Check for startxref line
    PDF_TRY(pdf_ctx_shift(ctx, -1));
    PDF_TRY(pdf_ctx_seek_line_start(ctx));
    PDF_TRY(pdf_ctx_expect(ctx, "startxref"));

    return PDF_OK;
}

#ifdef TEST
#include <string.h>

#include "test.h"

TEST_FUNC(test_header_valid) {
    char buffer[] = "%PDF-1.5";
    Arena* arena = arena_new(128);

    PdfCtx* ctx = pdf_ctx_new(arena, buffer, strlen(buffer));
    TEST_ASSERT(ctx);

    uint8_t version;
    TEST_ASSERT_EQ((PdfResult)PDF_OK, pdf_parse_header(ctx, &version));
    TEST_ASSERT_EQ((uint8_t)5, version);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_header_invalid) {
    char buffer[] = "hello";
    Arena* arena = arena_new(128);

    PdfCtx* ctx = pdf_ctx_new(arena, buffer, strlen(buffer));
    TEST_ASSERT(ctx);

    uint8_t version;
    TEST_ASSERT_EQ(
        (PdfResult)PDF_CTX_ERR_EXPECT,
        pdf_parse_header(ctx, &version)
    );

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_header_invalid_version) {
    char buffer[] = "%PDF-1.f";
    Arena* arena = arena_new(128);

    PdfCtx* ctx = pdf_ctx_new(arena, buffer, strlen(buffer));
    TEST_ASSERT(ctx);

    uint8_t version;
    TEST_ASSERT_EQ(
        (PdfResult)PDF_ERR_INVALID_VERSION,
        pdf_parse_header(ctx, &version)
    );

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_startxref) {
    char buffer[] = "startxref\n4325\n%%EOF";
    Arena* arena = arena_new(128);

    PdfCtx* ctx = pdf_ctx_new(arena, buffer, strlen(buffer));
    TEST_ASSERT(ctx);

    size_t startxref;
    TEST_ASSERT_EQ((PdfResult)PDF_OK, pdf_parse_startxref(ctx, &startxref));
    TEST_ASSERT_EQ((size_t)4325, startxref);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_startxref_invalid) {
    char buffer[] = "startxref\n\n%%EOF";
    Arena* arena = arena_new(128);

    PdfCtx* ctx = pdf_ctx_new(arena, buffer, strlen(buffer));
    TEST_ASSERT(ctx);

    size_t startxref;
    TEST_ASSERT_EQ(
        (PdfResult)PDF_CTX_ERR_INVALID_NUMBER,
        pdf_parse_startxref(ctx, &startxref)
    );

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_startxref_invalid2) {
    char buffer[] = "startxref\n+435\n%%EOF";
    Arena* arena = arena_new(128);

    PdfCtx* ctx = pdf_ctx_new(arena, buffer, strlen(buffer));
    TEST_ASSERT(ctx);

    size_t startxref;
    TEST_ASSERT_EQ(
        (PdfResult)PDF_CTX_ERR_INVALID_NUMBER,
        pdf_parse_startxref(ctx, &startxref)
    );

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_startxref_invalid3) {
    char buffer[] = "notstartxref\n4325\n%%EOF";
    Arena* arena = arena_new(128);

    PdfCtx* ctx = pdf_ctx_new(arena, buffer, strlen(buffer));
    TEST_ASSERT(ctx);

    size_t startxref;
    TEST_ASSERT_EQ(
        (PdfResult)PDF_CTX_ERR_EXPECT,
        pdf_parse_startxref(ctx, &startxref)
    );

    arena_free(arena);
    return TEST_RESULT_PASS;
}

#endif // TEST
