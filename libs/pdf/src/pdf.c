#include "pdf/pdf.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "arena/arena.h"
#include "ctx.h"
#include "deserialize.h"
#include "logger/log.h"
#include "object.h"
#include "pdf/catalog.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf/trailer.h"
#include "pdf_error/error.h"
#include "test_helpers.h"
#include "xref.h"

typedef struct {
    PdfIntegerOptional prev;
} StubTrailer;

static PdfError* deserialize_stub_trailer(
    const PdfObject* object,
    StubTrailer* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(resolver);
    RELEASE_ASSERT(target_ptr);

    PdfFieldDescriptor fields[] = {PDF_FIELD(
        "Prev",
        &target_ptr->prev,
        PDF_DESERDE_OPTIONAL(
            pdf_integer_op_init,
            PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_INTEGER)
        )
    )};

    PDF_PROPAGATE(pdf_deserialize_dict(
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        true,
        resolver,
        "PdfTrailer (prev-only)"
    ));

    return NULL;
}

struct PdfResolver {
    Arena* arena;
    PdfCtx* ctx;
    XRefTable* xref;

    uint8_t version;
    PdfTrailer trailer;
    PdfCatalog* catalog;
};

static PdfError* parse_header(PdfCtx* ctx, uint8_t* version);
static PdfError* parse_startxref(PdfCtx* ctx, size_t* startxref);
static PdfError*
parse_stub_trailer(PdfResolver* resolver, StubTrailer* trailer);
static PdfError* parse_trailer(PdfResolver* resolver, PdfTrailer* trailer);

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
    PDF_PROPAGATE(parse_header(ctx, &version));
    LOG_DIAG(INFO, DOC, "File Version 1.%hhu", version);

    size_t current_xref_offset;
    PDF_PROPAGATE(parse_startxref(ctx, &current_xref_offset));

    XRefTable* xref = pdf_xref_init(arena, ctx);
    *resolver = arena_alloc(arena, sizeof(PdfResolver));
    (*resolver)->arena = arena;
    (*resolver)->ctx = ctx;
    (*resolver)->xref = xref;
    (*resolver)->version = version;
    (*resolver)->catalog = NULL;

    size_t first_trailer_offset = SIZE_MAX;
    while (current_xref_offset != 0) {
        PDF_PROPAGATE(
            pdf_xref_parse_section(arena, ctx, current_xref_offset, xref)
        );

        if (first_trailer_offset == SIZE_MAX) {
            first_trailer_offset = pdf_ctx_offset(ctx);
        }

        StubTrailer trailer;
        PDF_PROPAGATE(parse_stub_trailer(*resolver, &trailer));

        if (trailer.prev.has_value) {
            current_xref_offset = (size_t)trailer.prev.value;
        } else {
            current_xref_offset = 0;
        }
    }

    if (first_trailer_offset == SIZE_MAX) {
        return PDF_ERROR(PDF_ERR_INVALID_TRAILER, "Trailer is missing");
    }

    PDF_PROPAGATE(pdf_ctx_seek(ctx, first_trailer_offset));
    PDF_PROPAGATE(parse_trailer(*resolver, &(*resolver)->trailer));

    return NULL;
}

#ifdef TEST
PdfResolver* pdf_fake_resolver_new(Arena* arena, PdfCtx* ctx) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(ctx);

    PdfResolver* resolver = arena_alloc(arena, sizeof(PdfResolver));
    resolver->arena = arena;
    resolver->ctx = ctx;
    resolver->version = 0;
    resolver->xref = NULL;
    resolver->catalog = NULL;

    return resolver;
}
#endif

Arena* pdf_resolver_arena(PdfResolver* resolver) {
    RELEASE_ASSERT(resolver);

    return resolver->arena;
}

PdfCtx* pdf_resolver_ctx(PdfResolver* resolver) {
    RELEASE_ASSERT(resolver);

    return resolver->ctx;
}

static PdfError*
parse_stub_trailer(PdfResolver* resolver, StubTrailer* trailer) {
    RELEASE_ASSERT(resolver);
    RELEASE_ASSERT(trailer);

    PDF_PROPAGATE(pdf_ctx_consume_whitespace(resolver->ctx));
    PDF_PROPAGATE(pdf_ctx_expect(resolver->ctx, "trailer"));
    PDF_PROPAGATE(pdf_ctx_seek_next_line(resolver->ctx));

    PdfObject trailer_object;
    PDF_PROPAGATE(pdf_parse_object(resolver, &trailer_object, false));

    printf("%s\n", pdf_fmt_object(resolver->arena, &trailer_object));

    PDF_PROPAGATE(deserialize_stub_trailer(&trailer_object, trailer, resolver));
    return NULL;
}

static PdfError* parse_trailer(PdfResolver* resolver, PdfTrailer* trailer) {
    RELEASE_ASSERT(resolver);
    RELEASE_ASSERT(trailer);

    PDF_PROPAGATE(pdf_ctx_consume_whitespace(resolver->ctx));
    PDF_PROPAGATE(pdf_ctx_expect(resolver->ctx, "trailer"));
    PDF_PROPAGATE(pdf_ctx_seek_next_line(resolver->ctx));

    PdfObject trailer_object;
    PDF_PROPAGATE(pdf_parse_object(resolver, &trailer_object, false));

    PDF_PROPAGATE(pdf_deserialize_trailer(&trailer_object, trailer, resolver));
    return NULL;
}

void pdf_get_trailer(PdfResolver* resolver, PdfTrailer* trailer) {
    RELEASE_ASSERT(resolver);
    RELEASE_ASSERT(trailer);

    *trailer = resolver->trailer;
}

PdfError* pdf_get_catalog(PdfResolver* resolver, PdfCatalog* catalog) {
    RELEASE_ASSERT(resolver);
    RELEASE_ASSERT(catalog);

    if (resolver->catalog) {
        *catalog = *resolver->catalog;
        return NULL;
    }

    PdfTrailer trailer;
    pdf_get_trailer(resolver, &trailer);
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
    PDF_PROPAGATE(pdf_parse_object(resolver, entry->object, false));
    *resolved = *entry->object;

    return NULL;
}

PdfError* pdf_resolve_object(
    PdfResolver* resolver,
    const PdfObject* object,
    PdfObject* resolved,
    bool strip_objs
) {
    RELEASE_ASSERT(resolver);
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(resolved);

    if (object->type == PDF_OBJECT_TYPE_INDIRECT_OBJECT && strip_objs) {
        LOG_DIAG(TRACE, PDF, "Unwrapping indirect object");
        return pdf_resolve_object(
            resolver,
            object->data.indirect_object.object,
            resolved,
            strip_objs
        );
    }

    if (object->type == PDF_OBJECT_TYPE_INDIRECT_REF) {
        LOG_DIAG(TRACE, PDF, "Resolving indirect reference");

        PdfObject indirect_object;
        PDF_PROPAGATE(pdf_resolve_ref(
            resolver,
            object->data.indirect_ref,
            &indirect_object
        ));

        return pdf_resolve_object(
            resolver,
            &indirect_object,
            resolved,
            strip_objs
        );
    }

    LOG_DIAG(TRACE, PDF, "Resolved type is %d", object->type);

    *resolved = *object;
    return NULL;
}

// The first line of a PDF file shall be a header consisting of the 5 characters
// %PDF– followed by a version number of the form 1.N, where N is a digit
// between 0 and 7.
PdfError* parse_header(PdfCtx* ctx, uint8_t* version) {
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
PdfError* parse_startxref(PdfCtx* ctx, size_t* startxref) {
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
    TEST_PDF_REQUIRE(parse_header(ctx, &version));
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
    TEST_PDF_REQUIRE_ERR(parse_header(ctx, &version), PDF_ERR_CTX_EXPECT);

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
    TEST_PDF_REQUIRE_ERR(parse_header(ctx, &version), PDF_ERR_INVALID_VERSION);

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
    TEST_PDF_REQUIRE(parse_startxref(ctx, &startxref));
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
        parse_startxref(ctx, &startxref),
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
        parse_startxref(ctx, &startxref),
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
    TEST_PDF_REQUIRE_ERR(parse_startxref(ctx, &startxref), PDF_ERR_CTX_EXPECT);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

#endif // TEST
