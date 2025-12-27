#include "pdf/pdf.h"

#include <stdint.h>
#include <stdlib.h>

#include "arena/arena.h"
#include "ctx.h"
#include "logger/log.h"
#include "object.h"
#include "pdf/catalog.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf/trailer.h"
#include "pdf_error/error.h"
#include "xref.h"

struct PdfResolver {
    Arena* arena;
    PdfCtx* ctx;

    uint8_t version;
    size_t startxref;
    XRefTable* xref;

    PdfTrailer* trailer;
    PdfCatalog* catalog;
};

PdfError* pdf_parse_header(PdfCtx* ctx, uint8_t* version);
PdfError* pdf_parse_startxref(PdfCtx* ctx, size_t* startxref);

PdfError* pdf_resolver_new(
    Arena* arena,
    const uint8_t* buffer,
    size_t buffer_size,
    PdfResolver** resolver
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(buffer);
    RELEASE_ASSERT(buffer_size != 0);
    RELEASE_ASSERT(resolver);

    PdfCtx* ctx = pdf_ctx_new(arena, buffer, buffer_size);

    uint8_t version;
    PDF_PROPAGATE(pdf_parse_header(ctx, &version));
    LOG_DIAG(INFO, DOC, "File Version 1.%hhu", version);

    size_t startxref;
    PDF_PROPAGATE(pdf_parse_startxref(ctx, &startxref));
    LOG_DIAG(DEBUG, DOC, "Startxref: %zu", startxref);

    XRefTable* xref;
    PDF_PROPAGATE(
        pdf_xref_new(arena, ctx, startxref, &xref),
        "Failed to create the xref table"
    );

    *resolver = arena_alloc(arena, sizeof(PdfResolver));
    (*resolver)->arena = arena;
    (*resolver)->ctx = ctx;
    (*resolver)->version = version;
    (*resolver)->startxref = startxref;
    (*resolver)->xref = xref;
    (*resolver)->trailer = NULL;
    (*resolver)->catalog = NULL;

    return NULL;
}

PdfOptionalResolver pdf_op_resolver_some(PdfResolver* resolver) {
    RELEASE_ASSERT(resolver);

    return (PdfOptionalResolver
    ) {.present = true, .unwrap_indirect_objs = true, .resolver = resolver};
}

PdfOptionalResolver pdf_op_resolver_none(bool unwrap_indirect_objs) {
    return (PdfOptionalResolver
    ) {.present = false,
       .unwrap_indirect_objs = unwrap_indirect_objs,
       .resolver = NULL};
}

bool pdf_op_resolver_valid(PdfOptionalResolver resolver) {
    return !resolver.present || resolver.resolver;
}

Arena* pdf_resolver_arena(PdfResolver* resolver) {
    RELEASE_ASSERT(resolver);

    return resolver->arena;
}

PdfError* pdf_get_trailer(PdfResolver* resolver, PdfTrailer* trailer) {
    RELEASE_ASSERT(resolver);
    RELEASE_ASSERT(trailer);

    if (resolver->trailer) {
        *trailer = *resolver->trailer;
        return NULL;
    }

    PDF_PROPAGATE(pdf_ctx_seek(resolver->ctx, pdf_ctx_buffer_len(resolver->ctx))
    );
    PDF_PROPAGATE(pdf_ctx_seek_line_start(resolver->ctx));

    while (!pdf_error_free_is_ok(pdf_ctx_expect(resolver->ctx, "trailer"))
           && pdf_ctx_offset(resolver->ctx) != 0) {
        PDF_PROPAGATE(pdf_ctx_shift(resolver->ctx, -1));
        PDF_PROPAGATE(pdf_ctx_seek_line_start(resolver->ctx));
    }

    PDF_PROPAGATE(pdf_ctx_seek_next_line(resolver->ctx));

    PdfObject* trailer_dict = arena_alloc(resolver->arena, sizeof(PdfObject));
    PDF_PROPAGATE(pdf_parse_object(
        resolver->arena,
        resolver->ctx,
        pdf_op_resolver_some(resolver),
        trailer_dict,
        false
    ));

    PDF_PROPAGATE(pdf_deserialize_trailer(
        trailer_dict,
        trailer,
        pdf_op_resolver_some(resolver),
        resolver->arena
    ));

    resolver->trailer = arena_alloc(resolver->arena, sizeof(PdfTrailer));
    *resolver->trailer = *trailer;

    return NULL;
}

PdfError* pdf_get_catalog(PdfResolver* resolver, PdfCatalog* catalog) {
    RELEASE_ASSERT(resolver);
    RELEASE_ASSERT(catalog);

    if (resolver->catalog) {
        *catalog = *resolver->catalog;
        return NULL;
    }

    PdfTrailer trailer;
    PDF_PROPAGATE(pdf_get_trailer(resolver, &trailer));
    PDF_PROPAGATE(pdf_resolve_catalog(trailer.root, resolver, catalog));

    resolver->catalog = arena_alloc(resolver->arena, sizeof(PdfCatalog));
    *resolver->catalog = *catalog;

    return NULL;
}

PdfError* pdf_resolve_ref(
    PdfResolver* resolver,
    PdfIndirectRef ref,
    PdfObject* resolved
) {
    RELEASE_ASSERT(resolver);
    RELEASE_ASSERT(resolved);

    XRefEntry* entry;
    PDF_PROPAGATE(pdf_xref_get_entry(
        resolver->xref,
        ref.object_id,
        ref.generation,
        &entry
    ));

    PDF_PROPAGATE(pdf_ctx_seek(resolver->ctx, entry->offset));

    entry->object = arena_alloc(resolver->arena, sizeof(PdfObject));
    PDF_PROPAGATE(pdf_parse_object(
        resolver->arena,
        resolver->ctx,
        pdf_op_resolver_some(resolver),
        entry->object,
        false
    ));
    *resolved = *entry->object;

    return NULL;
}

PdfError* pdf_resolve_object(
    PdfOptionalResolver resolver,
    const PdfObject* object,
    PdfObject* resolved
) {
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(resolved);

    if (object->type == PDF_OBJECT_TYPE_INDIRECT_OBJECT
        && resolver.unwrap_indirect_objs) {
        LOG_DIAG(TRACE, PDF, "Unwrapping indirect object");
        return pdf_resolve_object(
            resolver,
            object->data.indirect_object.object,
            resolved
        );
    }

    if (object->type == PDF_OBJECT_TYPE_INDIRECT_REF && resolver.present) {
        LOG_DIAG(TRACE, PDF, "Resolving indirect reference");

        PdfObject indirect_object;
        PDF_PROPAGATE(pdf_resolve_ref(
            resolver.resolver,
            object->data.indirect_ref,
            &indirect_object
        ));

        return pdf_resolve_object(resolver, &indirect_object, resolved);
    }

    LOG_DIAG(TRACE, PDF, "Resolved type is %d", object->type);

    *resolved = *object;
    return NULL;
}

// The first line of a PDF file shall be a header consisting of the 5 characters
// %PDF– followed by a version number of the form 1.N, where N is a digit
// between 0 and 7.
PdfError* pdf_parse_header(PdfCtx* ctx, uint8_t* version) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(version);

    uint8_t version_byte;
    PDF_PROPAGATE(pdf_ctx_expect(ctx, "%PDF-1."));
    PDF_PROPAGATE(pdf_ctx_peek_and_advance(ctx, &version_byte));

    if (version_byte < '0' || version_byte > '7') {
        *version = 0;
        return PDF_ERROR(
            PDF_ERR_INVALID_VERSION,
            "Only PDF versions 1.0 to 1.7 supported"
        );
    } else {
        *version = (uint8_t)(version_byte - '0');
    }

    return NULL;
}

// The last line of the file shall contain only the end-of-file marker, %%EOF.
// The two preceding lines shall contain, one per line and in order, the keyword
// startxref and the byte offset in the decoded stream from the beginning of the
// file to the beginning of the xref keyword in the last cross-reference
// section.
PdfError* pdf_parse_startxref(PdfCtx* ctx, size_t* startxref) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(startxref);

    // Find EOF marker
    PDF_PROPAGATE(pdf_ctx_seek(ctx, pdf_ctx_buffer_len(ctx)));
    PDF_PROPAGATE(pdf_ctx_backscan(ctx, "%%EOF", 32));

    size_t eof_marker_offset = pdf_ctx_offset(ctx);
    PDF_PROPAGATE(pdf_ctx_seek_line_start(ctx));
    if (eof_marker_offset != pdf_ctx_offset(ctx)) {
        return PDF_ERROR(
            PDF_ERR_INVALID_TRAILER,
            "Pdf file EOF marker not aligned to start of line"
        );
    }

    // Parse byte offset
    PDF_PROPAGATE(pdf_ctx_shift(ctx, -1));
    PDF_PROPAGATE(pdf_ctx_seek_line_start(ctx));

    uint64_t xref_offset;
    uint32_t int_length;
    PDF_PROPAGATE(pdf_ctx_parse_int(ctx, NULL, &xref_offset, &int_length));
    if (int_length == 0) {
        return PDF_ERROR(PDF_ERR_INVALID_STARTXREF);
    }

    *startxref = (size_t)xref_offset;

    // Check for startxref line
    PDF_PROPAGATE(pdf_ctx_seek_line_start(ctx));
    PDF_PROPAGATE(pdf_ctx_shift(ctx, -1));
    PDF_PROPAGATE(pdf_ctx_seek_line_start(ctx));
    PDF_PROPAGATE(pdf_ctx_expect(ctx, "startxref"));

    return NULL;
}

#ifdef TEST
#include <string.h>

#include "test/test.h"

TEST_FUNC(test_header_valid) {
    uint8_t buffer[] = "%PDF-1.5";
    Arena* arena = arena_new(128);

    PdfCtx* ctx =
        pdf_ctx_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    TEST_ASSERT(ctx);

    uint8_t version = 0;
    TEST_PDF_REQUIRE(pdf_parse_header(ctx, &version));
    TEST_ASSERT_EQ((uint8_t)5, version);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_header_invalid) {
    uint8_t buffer[] = "hello";
    Arena* arena = arena_new(128);

    PdfCtx* ctx =
        pdf_ctx_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    TEST_ASSERT(ctx);

    uint8_t version;
    TEST_PDF_REQUIRE_ERR(pdf_parse_header(ctx, &version), PDF_ERR_CTX_EXPECT);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_header_invalid_version) {
    uint8_t buffer[] = "%PDF-1.f";
    Arena* arena = arena_new(128);

    PdfCtx* ctx =
        pdf_ctx_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    TEST_ASSERT(ctx);

    uint8_t version;
    TEST_PDF_REQUIRE_ERR(
        pdf_parse_header(ctx, &version),
        PDF_ERR_INVALID_VERSION
    );

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_startxref) {
    uint8_t buffer[] = "startxref\n4325\n%%EOF";
    Arena* arena = arena_new(128);

    PdfCtx* ctx =
        pdf_ctx_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    TEST_ASSERT(ctx);

    size_t startxref;
    TEST_PDF_REQUIRE(pdf_parse_startxref(ctx, &startxref));
    TEST_ASSERT_EQ((size_t)4325, startxref);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_startxref_invalid) {
    uint8_t buffer[] = "startxref\n\n%%EOF";
    Arena* arena = arena_new(128);

    PdfCtx* ctx =
        pdf_ctx_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    TEST_ASSERT(ctx);

    size_t startxref;
    TEST_PDF_REQUIRE_ERR(
        pdf_parse_startxref(ctx, &startxref),
        PDF_ERR_INVALID_STARTXREF
    );

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_startxref_invalid2) {
    uint8_t buffer[] = "startxref\n+435\n%%EOF";
    Arena* arena = arena_new(128);

    PdfCtx* ctx =
        pdf_ctx_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    TEST_ASSERT(ctx);

    size_t startxref;
    TEST_PDF_REQUIRE_ERR(
        pdf_parse_startxref(ctx, &startxref),
        PDF_ERR_INVALID_STARTXREF
    );

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_startxref_invalid3) {
    uint8_t buffer[] = "notstartxref\n4325\n%%EOF";
    Arena* arena = arena_new(128);

    PdfCtx* ctx =
        pdf_ctx_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    TEST_ASSERT(ctx);

    size_t startxref;
    TEST_PDF_REQUIRE_ERR(
        pdf_parse_startxref(ctx, &startxref),
        PDF_ERR_CTX_EXPECT
    );

    arena_free(arena);
    return TEST_RESULT_PASS;
}

#endif // TEST
