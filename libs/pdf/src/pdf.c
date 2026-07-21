#include "pdf/pdf.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "arena/arena.h"
#include "ctx.h"
#include "err/error.h"
#include "logger/log.h"
#include "object.h"
#include "object_stream.h"
#include "pdf/catalog.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf/trailer.h"
#include "pdf/types.h"
#include "test_helpers.h"
#include "xref.h"

typedef struct {
    PdfIntegerOptional prev;
    PdfIntegerOptional xref_stm;
} StubTrailer;

static Error* deserde_stub_trailer(
    const PdfObject* object,
    StubTrailer* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(resolver);
    RELEASE_ASSERT(target_ptr);

    PdfFieldDescriptor fields[] = {
        pdf_integer_optional_field("Prev", &target_ptr->prev),
        pdf_integer_optional_field("XRefStm", &target_ptr->xref_stm)
    };

    TRY(pdf_deserde_fields(
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

static Error* parse_header(PdfCtx* ctx, uint8_t* version);
static Error* parse_startxref(PdfCtx* ctx, size_t* startxref);

Error* pdf_resolver_new(
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
    TRY(parse_header(ctx, &version));
    LOG_DIAG(INFO, DOC, "File Version 1.%hhu", version);

    size_t current_xref_offset;
    TRY(parse_startxref(ctx, &current_xref_offset));

    XRefTable* xref = pdf_xref_init(arena, ctx);
    *resolver = arena_alloc(arena, sizeof(PdfResolver));
    (*resolver)->arena = arena;
    (*resolver)->ctx = ctx;
    (*resolver)->xref = xref;
    (*resolver)->version = version;
    (*resolver)->catalog = NULL;

    PdfObject first_trailer_dict;
    bool has_trailer = false;
    while (current_xref_offset != 0) {
        PdfObject trailer_dict;
        TRY(pdf_xref_parse_section(
            *resolver,
            current_xref_offset,
            xref,
            &trailer_dict
        ));

        if (!has_trailer) {
            first_trailer_dict = trailer_dict;
            has_trailer = true;
        }

        StubTrailer trailer;
        TRY(deserde_stub_trailer(&trailer_dict, &trailer, *resolver));

        // XRefStm is parsed before Prev so its entries take priority (7.5.8.4)
        if (trailer.xref_stm.is_some) {
            PdfObject xref_stm_dict;
            TRY(pdf_xref_parse_section(
                *resolver,
                (size_t)trailer.xref_stm.value,
                xref,
                &xref_stm_dict
            ));
        }

        if (trailer.prev.is_some) {
            current_xref_offset = (size_t)trailer.prev.value;
        } else {
            current_xref_offset = 0;
        }
    }

    if (!has_trailer) {
        return ERROR(PDF_ERR_INVALID_TRAILER, "Trailer is missing");
    }

    TRY(pdf_deserde_trailer(
        &first_trailer_dict,
        &(*resolver)->trailer,
        *resolver
    ));

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

PdfCtx* pdf_resolver_swap_ctx(PdfResolver* resolver, PdfCtx* ctx) {
    RELEASE_ASSERT(resolver);
    RELEASE_ASSERT(ctx);

    PdfCtx* previous = resolver->ctx;
    resolver->ctx = ctx;
    return previous;
}

void pdf_get_trailer(PdfResolver* resolver, PdfTrailer* trailer) {
    RELEASE_ASSERT(resolver);
    RELEASE_ASSERT(trailer);

    *trailer = resolver->trailer;
}

Error* pdf_get_catalog(PdfResolver* resolver, PdfCatalog* catalog) {
    RELEASE_ASSERT(resolver);
    RELEASE_ASSERT(catalog);

    if (resolver->catalog) {
        *catalog = *resolver->catalog;
        return NULL;
    }

    PdfTrailer trailer;
    pdf_get_trailer(resolver, &trailer);
    TRY(pdf_resolve_catalog(&trailer.root, resolver));

    resolver->catalog =
        arena_alloc(pdf_resolver_arena(resolver), sizeof(PdfCatalog));
    *resolver->catalog = *trailer.root.resolved;
    *catalog = *resolver->catalog;

    return NULL;
}

Error* pdf_resolve_ref(
    PdfResolver* resolver,
    PdfIndirectRef ref,
    PdfObject* resolved
) {
    RELEASE_ASSERT(resolver);
    RELEASE_ASSERT(resolved);

    XRefEntry* entry;
    TRY(pdf_xref_get_entry(
        resolver->xref,
        ref.object_id,
        ref.generation,
        &entry
    ));

    switch (entry->type) {
        case XREF_ENTRY_FREE: {
            // An indirect reference to an undefined object shall not be
            // considered an error by a conforming reader; it shall be treated
            // as a reference to the null object
            resolved->type = PDF_OBJECT_TYPE_NULL;
            return NULL;
        }
        case XREF_ENTRY_IN_USE: {
            TRY(pdf_ctx_seek(resolver->ctx, entry->offset));

            entry->object = arena_alloc(resolver->arena, sizeof(PdfObject));
            TRY(pdf_parse_object(resolver, entry->object, false));
            *resolved = *entry->object;

            return NULL;
        }
        case XREF_ENTRY_COMPRESSED: {
            // The generation number of an object stream and of any compressed
            // object shall be zero
            PdfObject container;
            TRY(pdf_resolve_ref(
                resolver,
                (PdfIndirectRef) {.object_id = entry->stream_object_id,
                                  .generation = 0},
                &container
            ));

            entry->object = arena_alloc(resolver->arena, sizeof(PdfObject));
            TRY(pdf_object_stream_get(
                resolver,
                &container,
                entry->stream_index,
                ref.object_id,
                entry->object
            ));
            *resolved = *entry->object;

            return NULL;
        }
    }

    LOG_PANIC("Unreachable");
}

Error* pdf_resolve_object(
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
        TRY(pdf_resolve_ref(
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
Error* parse_header(PdfCtx* ctx, uint8_t* version) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(version);

    uint8_t version_byte;
    TRY(pdf_ctx_expect(ctx, "%PDF-1."));
    TRY(pdf_ctx_peek_and_advance(ctx, &version_byte));

    if (version_byte < '0' || version_byte > '7') {
        *version = 0;
        return ERROR(
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
Error* parse_startxref(PdfCtx* ctx, size_t* startxref) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(startxref);

    // Find EOF marker
    TRY(pdf_ctx_seek(ctx, pdf_ctx_buffer_len(ctx)));
    TRY(pdf_ctx_backscan(ctx, "%%EOF", 32));

    size_t eof_marker_offset = pdf_ctx_offset(ctx);
    TRY(pdf_ctx_seek_line_start(ctx));
    if (eof_marker_offset != pdf_ctx_offset(ctx)) {
        return ERROR(
            PDF_ERR_INVALID_TRAILER,
            "Pdf file EOF marker not aligned to start of line"
        );
    }

    // Parse byte offset
    TRY(pdf_ctx_shift(ctx, -1));
    TRY(pdf_ctx_seek_line_start(ctx));

    uint64_t xref_offset;
    uint32_t int_length;
    TRY(pdf_ctx_parse_int(ctx, NULL, &xref_offset, &int_length));
    if (int_length == 0) {
        return ERROR(PDF_ERR_INVALID_STARTXREF);
    }

    *startxref = (size_t)xref_offset;

    // Check for startxref line
    TRY(pdf_ctx_seek_line_start(ctx));
    TRY(pdf_ctx_shift(ctx, -1));
    TRY(pdf_ctx_seek_line_start(ctx));
    TRY(pdf_ctx_expect(ctx, "startxref"));

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
    TEST_REQUIRE(parse_header(ctx, &version));
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
    TEST_REQUIRE_ERR(parse_header(ctx, &version), PDF_ERR_CTX_EXPECT);

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
    TEST_REQUIRE_ERR(parse_header(ctx, &version), PDF_ERR_INVALID_VERSION);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_resolver_xref_stream) {
    uint8_t buffer[] =
        "%PDF-1.5\n"
        "4 0 obj\n<</Type/ObjStm/N 1/First 5/Length 34>>\nstream\n"
        "1 0 \n<</Type/Catalog/Pages 2 0 R>>"
        "\nendstream\nendobj\n"
        "3 0 obj\n<</Type/XRef/Size 5/W[1 2 1]/Root 1 0 R/Length 20>>\nstream\n"
        "\x00\x00\x00\xff"
        "\x02\x00\x04\x00"
        "\x00\x00\x00\x00"
        "\x01\x00\x73\x00"
        "\x01\x00\x09\x00"
        "\nendstream\nendobj\n"
        "startxref\n115\n%%EOF";
    Arena* arena = arena_new(1024);

    PdfResolver* resolver;
    TEST_REQUIRE(pdf_resolver_new(
        arena,
        buffer,
        sizeof(buffer) / sizeof(uint8_t) - 1,
        &resolver
    ));

    PdfTrailer trailer;
    pdf_get_trailer(resolver, &trailer);
    TEST_ASSERT_EQ(5, trailer.size);
    TEST_ASSERT_EQ((size_t)1, trailer.root.ref.object_id);

    PdfObject catalog;
    TEST_REQUIRE(pdf_resolve_ref(
        resolver,
        (PdfIndirectRef) {.object_id = 1, .generation = 0},
        &catalog
    ));
    TEST_ASSERT(catalog.type == PDF_OBJECT_TYPE_DICT);

    PdfObject freed;
    TEST_REQUIRE(pdf_resolve_ref(
        resolver,
        (PdfIndirectRef) {.object_id = 0, .generation = 255},
        &freed
    ));
    TEST_ASSERT(freed.type == PDF_OBJECT_TYPE_NULL);

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
    TEST_REQUIRE(parse_startxref(ctx, &startxref));
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
    TEST_REQUIRE_ERR(
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
    TEST_REQUIRE_ERR(
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
    TEST_REQUIRE_ERR(parse_startxref(ctx, &startxref), PDF_ERR_CTX_EXPECT);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

#endif // TEST
