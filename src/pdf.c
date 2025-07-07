#include "pdf.h"

#include <stdint.h>
#include <stdlib.h>

#include "arena.h"
#include "ctx.h"
#include "log.h"
#include "object.h"
#include "pdf_object.h"
#include "pdf_result.h"
#include "vec.h"
#include "xref.h"

struct PdfDocument {
    Arena* arena;
    PdfCtx* ctx;

    uint8_t version;
    size_t startxref;
    XRefTable* xref;

    PdfObject* trailer;
    PdfObject* root;
};

PdfResult pdf_parse_header(PdfCtx* ctx, uint8_t* version);
PdfResult pdf_parse_startxref(PdfCtx* ctx, size_t* startxref);

PdfDocument* pdf_document_new(
    Arena* arena,
    char* buffer,
    size_t buffer_size,
    PdfResult* result
) {
    if (!result || !arena || !buffer) {
        LOG_WARN("Invalid args to pdf_document new");
        return NULL;
    }

    PdfCtx* ctx = pdf_ctx_new(arena, buffer, buffer_size);

    uint8_t version;
    PDF_TRY_RET_NULL(pdf_parse_header(ctx, &version));
    LOG_INFO_G("doc-info", "File Version 1.%hhu", version);

    size_t startxref;
    PDF_TRY_RET_NULL(pdf_parse_startxref(ctx, &startxref));
    LOG_DEBUG_G("doc-info", "Startxref: %zu", startxref);

    PdfResult xref_result = PDF_OK;
    XRefTable* xref = pdf_xref_new(arena, ctx, startxref, &xref_result);
    if (xref_result != PDF_OK) {
        LOG_ERROR("Failed to create xref table");
        *result = xref_result;
        return NULL;
    }

    PdfDocument* doc = arena_alloc(arena, sizeof(PdfDocument));
    doc->arena = arena;
    doc->ctx = ctx;
    doc->version = version;
    doc->startxref = startxref;
    doc->xref = xref;
    doc->trailer = NULL;
    doc->root = NULL;

    *result = PDF_OK;
    return doc;
}

PdfObject* pdf_get_trailer(PdfDocument* doc, PdfResult* result) {
    if (!result || !doc) {
        LOG_ERROR("Invalid args to pdf_get_trailer");
        return NULL;
    }

    *result = PDF_OK;

    if (doc->trailer) {
        return doc->trailer;
    }

    PDF_TRY_RET_NULL(pdf_ctx_seek(doc->ctx, pdf_ctx_buffer_len(doc->ctx)));
    PDF_TRY_RET_NULL(pdf_ctx_seek_line_start(doc->ctx));

    while (pdf_ctx_expect(doc->ctx, "trailer") != PDF_OK
           && pdf_ctx_offset(doc->ctx) != 0) {
        PDF_TRY_RET_NULL(pdf_ctx_shift(doc->ctx, -1));
        PDF_TRY_RET_NULL(pdf_ctx_seek_line_start(doc->ctx));
    }

    PDF_TRY_RET_NULL(pdf_ctx_seek_next_line(doc->ctx));

    doc->trailer = pdf_parse_object(doc->arena, doc->ctx, result, false);
    return doc->trailer;
}

PdfObject* pdf_get_root(PdfDocument* doc, PdfResult* result) {
    if (!result || !doc) {
        LOG_ERROR("Invalid args to pdf_get_root");
        return NULL;
    }

    *result = PDF_OK;

    if (doc->root) {
        return doc->root;
    }

    PdfObject* trailer = pdf_get_trailer(doc, result);
    if (*result != PDF_OK || !trailer) {
        return NULL;
    }

    PdfObject* root_ref = pdf_object_dict_get(trailer, "Root", result);
    if (*result != PDF_OK || !root_ref) {
        return NULL;
    }

    PdfObject* root = pdf_get_ref(doc, root_ref->data.ref_data, result);
    if (*result != PDF_OK || !root) {
        return NULL;
    }

    doc->root = root;
    return root;
}

PdfObject* pdf_get_ref(PdfDocument* doc, PdfObjectRef ref, PdfResult* result) {
    if (!doc || !result) {
        LOG_ERROR("Invalid args to pdf_get_ref");
        return NULL;
    }

    XRefEntry* entry =
        pdf_xref_get_entry(doc->xref, ref.object_id, ref.generation, result);
    if (*result != PDF_OK || !entry) {
        return NULL;
    }

    PDF_TRY_RET_NULL(pdf_ctx_seek(doc->ctx, entry->offset));
    entry->object = pdf_parse_object(doc->arena, doc->ctx, result, false);
    if (*result != PDF_OK || !entry->object) {
        return NULL;
    }

    return entry->object;
}

// The first line of a PDF file shall be a header consisting of the 5 characters
// %PDF– followed by a version number of the form 1.N, where N is a digit
// between 0 and 7.
PdfResult pdf_parse_header(PdfCtx* ctx, uint8_t* version) {
    DBG_ASSERT(version);

    char version_char;
    PDF_TRY(pdf_ctx_expect(ctx, "%PDF-1."));
    PDF_TRY(pdf_ctx_peek_and_advance(ctx, &version_char));

    if (version_char < '0' || version_char > '7') {
        *version = 0;
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

    // Parse byte offset
    PDF_TRY(pdf_ctx_shift(ctx, -1));
    PDF_TRY(pdf_ctx_seek_line_start(ctx));

    uint64_t xref_offset;
    uint32_t int_length;
    PDF_TRY(pdf_ctx_parse_int(ctx, NULL, &xref_offset, &int_length));
    if (int_length == 0) {
        return PDF_ERR_INVALID_STARTXREF;
    }

    *startxref = (size_t)xref_offset;

    // Check for startxref line
    PDF_TRY(pdf_ctx_seek_line_start(ctx));
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

    uint8_t version = 0;
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
        (PdfResult)PDF_ERR_INVALID_STARTXREF,
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
        (PdfResult)PDF_ERR_INVALID_STARTXREF,
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

TEST_FUNC(test_trailer) {
    char buffer[] =
        "%PDF-1.7\nxref\n0 1\n0000000000 65536 f\ntrailer\n<</Size 1>>\nstartxref\n9\n%%EOF";
    Arena* arena = arena_new(128);

    PdfResult result = PDF_OK;
    PdfDocument* doc = pdf_document_new(arena, buffer, strlen(buffer), &result);
    TEST_ASSERT_EQ((PdfResult)PDF_OK, result);
    TEST_ASSERT(doc);

    PdfObject* trailer = pdf_get_trailer(doc, &result);
    TEST_ASSERT_EQ((PdfResult)PDF_OK, result);
    TEST_ASSERT(trailer);
    TEST_ASSERT_EQ((PdfObjectKind)PDF_OBJECT_KIND_DICT, trailer->kind);

    Vec* trailer_entries = trailer->data.dict_data;
    TEST_ASSERT_EQ((size_t)1, vec_len(trailer->data.dict_data));

    PdfObjectDictEntry* entry = vec_get(trailer_entries, 0);
    TEST_ASSERT(entry);
    TEST_ASSERT_EQ((PdfObjectKind)PDF_OBJECT_KIND_NAME, entry->key->kind);
    TEST_ASSERT_EQ("Size", entry->key->data.string_data);
    TEST_ASSERT_EQ((PdfObjectKind)PDF_OBJECT_KIND_INTEGER, entry->value->kind);
    TEST_ASSERT_EQ((int32_t)1, entry->value->data.integer_data);

    return TEST_RESULT_PASS;
}

#endif // TEST
