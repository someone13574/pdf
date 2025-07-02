#include "parse.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

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

    char* xref_offset_str;
    PDF_TRY(pdf_ctx_borrow_substr(
        ctx,
        startxref_offset,
        eof_marker_offset - startxref_offset,
        &xref_offset_str
    ));

    if (xref_offset_str[0] == '+') { // stroll will parse signs
        return PDF_ERR_INVALID_TRAILER;
    }

    char* strol_end;
    long long xref_offset = strtoll(xref_offset_str, &strol_end, 10);
    if (xref_offset_str == strol_end || xref_offset < 0) {
        return PDF_ERR_INVALID_TRAILER;
    }

    if (errno == ERANGE) {
        return PDF_ERR_INVALID_TRAILER;
    }

    *startxref = (size_t)xref_offset;
    PDF_TRY(pdf_ctx_release_substr(ctx));

    // Check for startxref line
    PDF_TRY(pdf_ctx_shift(ctx, -1));
    PDF_TRY(pdf_ctx_seek_line_start(ctx));
    PDF_TRY(pdf_ctx_expect(ctx, "startxref"));

    return PDF_OK;
}

#ifdef TEST
#include "test.h"

TEST_FUNC(test_valid_header) {
    char buffer[] = "%PDF-1.5";
    PdfCtx* ctx = pdf_ctx_new(buffer, strlen(buffer));
    TEST_ASSERT(ctx, "Creation failed");

    uint8_t version;
    TEST_ASSERT_EQ((PdfResult)PDF_OK, pdf_parse_header(ctx, &version));
    TEST_ASSERT_EQ((uint8_t)5, version);

    pdf_ctx_free(ctx);

    return TEST_SUCCESS;
}

TEST_FUNC(test_invalid_header) {
    char buffer[] = "hello";
    PdfCtx* ctx = pdf_ctx_new(buffer, strlen(buffer));
    TEST_ASSERT(ctx, "Creation failed");

    uint8_t version;
    TEST_ASSERT_EQ(
        (PdfResult)PDF_CTX_ERR_EXPECT,
        pdf_parse_header(ctx, &version)
    );

    pdf_ctx_free(ctx);

    return TEST_SUCCESS;
}

TEST_FUNC(test_invalid_version) {
    char buffer[] = "%PDF-1.f";
    PdfCtx* ctx = pdf_ctx_new(buffer, strlen(buffer));
    TEST_ASSERT(ctx, "Creation failed");

    uint8_t version;
    TEST_ASSERT_EQ(
        (PdfResult)PDF_ERR_INVALID_VERSION,
        pdf_parse_header(ctx, &version)
    );

    pdf_ctx_free(ctx);

    return TEST_SUCCESS;
}

TEST_FUNC(test_parse_startxref) {
    char buffer[] = "startxref\n4325\n%%EOF";
    PdfCtx* ctx = pdf_ctx_new(buffer, strlen(buffer));
    TEST_ASSERT(ctx, "Creation failed");

    size_t startxref;
    TEST_ASSERT_EQ((PdfResult)PDF_OK, pdf_parse_startxref(ctx, &startxref));
    TEST_ASSERT_EQ((size_t)4325, startxref);

    pdf_ctx_free(ctx);

    return TEST_SUCCESS;
}

TEST_FUNC(test_parse_startxref_invalid) {
    char buffer[] = "startxref\n\n%%EOF";
    PdfCtx* ctx = pdf_ctx_new(buffer, strlen(buffer));
    TEST_ASSERT(ctx, "Creation failed");

    size_t startxref;
    TEST_ASSERT_EQ(
        (PdfResult)PDF_ERR_INVALID_TRAILER,
        pdf_parse_startxref(ctx, &startxref)
    );

    pdf_ctx_free(ctx);

    return TEST_SUCCESS;
}

TEST_FUNC(test_parse_startxref_invalid2) {
    char buffer[] = "startxref\n+435\n%%EOF";
    PdfCtx* ctx = pdf_ctx_new(buffer, strlen(buffer));
    TEST_ASSERT(ctx, "Creation failed");

    size_t startxref;
    TEST_ASSERT_EQ(
        (PdfResult)PDF_ERR_INVALID_TRAILER,
        pdf_parse_startxref(ctx, &startxref)
    );

    pdf_ctx_free(ctx);

    return TEST_SUCCESS;
}

TEST_FUNC(test_parse_startxref_invalid3) {
    char buffer[] = "notstartxref\n4325\n%%EOF";
    PdfCtx* ctx = pdf_ctx_new(buffer, strlen(buffer));
    TEST_ASSERT(ctx, "Creation failed");

    size_t startxref;
    TEST_ASSERT_EQ(
        (PdfResult)PDF_CTX_ERR_EXPECT,
        pdf_parse_startxref(ctx, &startxref)
    );

    pdf_ctx_free(ctx);

    return TEST_SUCCESS;
}

#endif // TEST
