#include "xref.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena/arena.h"
#include "ctx.h"
#include "logger/log.h"
#include "pdf_error/error.h"

typedef struct {
    size_t start_offset;
    size_t first_object;
    size_t num_entries;
    XRefEntry* entries;
} XRefSubsection;

#define DVEC_NAME XRefSubsectionVec
#define DVEC_LOWERCASE_NAME xref_subsection_vec
#define DVEC_TYPE XRefSubsection
#include "arena/dvec_impl.h"

struct XRefTable {
    Arena* arena;
    PdfCtx* ctx;
    XRefSubsectionVec* subsections;
};

// Each cross-reference subsection shall contain entries for a contiguous range
// of object numbers. The subsection shall begin with a line containing two
// numbers separated by a SPACE (20h), denoting the object number of the first
// object in this subsection and the number of entries in the subsection.
PdfError* pdf_xref_parse_subsection_header(
    PdfCtx* ctx,
    uint64_t* first_object,
    uint64_t* num_objects,
    size_t* subsection_start
) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(first_object);
    RELEASE_ASSERT(num_objects);
    RELEASE_ASSERT(subsection_start);

    // Parse object index
    uint32_t int_length;
    PDF_PROPAGATE(pdf_ctx_parse_int(ctx, NULL, first_object, &int_length));
    if (int_length == 0) {
        return PDF_ERROR(
            PDF_ERR_INVALID_XREF,
            "Expected an integer denoting the object number of the first object"
        );
    }

    PDF_PROPAGATE(pdf_ctx_expect(ctx, " "));

    // Parse length
    PDF_PROPAGATE(pdf_ctx_parse_int(ctx, NULL, num_objects, &int_length));
    if (int_length == 0) {
        return PDF_ERROR(
            PDF_ERR_INVALID_XREF,
            "Expected an integer denoting the subsection length"
        );
    }

    // Find start of subsection
    PDF_PROPAGATE(pdf_ctx_seek_next_line(ctx));
    *subsection_start = pdf_ctx_offset(ctx);

    return NULL;
}

PdfError* pdf_xref_parse_entry(
    Arena* arena,
    PdfCtx* ctx,
    XRefSubsection* subsection,
    size_t entry
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(subsection);
    RELEASE_ASSERT(entry < subsection->num_entries);

    if (!subsection->entries) {
        LOG_DIAG(
            DEBUG,
            XREF,
            "Initializing entries table for subsection with %zu entries",
            subsection->num_entries
        );

        subsection->entries =
            arena_alloc(arena, sizeof(XRefEntry) * subsection->num_entries);
        for (size_t idx = 0; idx < subsection->num_entries; idx++) {
            subsection->entries[idx].entry_parsed = false;
            subsection->entries[idx].object = NULL;
        }
    }

    // Seek entry
    size_t entry_offset = subsection->start_offset + 20 * entry;
    PDF_PROPAGATE(pdf_ctx_seek(ctx, entry_offset));

    // Check if start of line
    PDF_PROPAGATE(pdf_ctx_seek_line_start(ctx));
    if (pdf_ctx_offset(ctx) != entry_offset) {
        return PDF_ERROR(
            PDF_ERR_INVALID_XREF,
            "XRef entry not aligned to line start"
        );
    }

    // Parse offset
    uint64_t offset;
    uint32_t expected_length = 10;
    PDF_PROPAGATE(pdf_ctx_parse_int(ctx, &expected_length, &offset, NULL));
    PDF_PROPAGATE(pdf_ctx_expect(ctx, " "));

    // Parse generation
    uint64_t generation;
    expected_length = 5;
    PDF_PROPAGATE(pdf_ctx_parse_int(ctx, &expected_length, &generation, NULL));

    subsection->entries[entry].offset = (size_t)offset;
    subsection->entries[entry].generation = (size_t)generation;
    subsection->entries[entry].entry_parsed = true;

    return NULL;
}

XRefTable* pdf_xref_init(Arena* arena, PdfCtx* ctx) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(ctx);

    XRefTable* xref = arena_alloc(arena, sizeof(XRefTable));
    xref->arena = arena;
    xref->ctx = ctx;
    xref->subsections = xref_subsection_vec_new(arena);

    return xref;
}

PdfError* pdf_xref_parse_section(
    Arena* arena,
    PdfCtx* ctx,
    size_t xrefstart,
    XRefTable* xref
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(xref);

    // Validate xrefstart
    PDF_PROPAGATE(pdf_ctx_seek(ctx, xrefstart));
    PDF_PROPAGATE(pdf_ctx_expect(ctx, "xref"));

    // Seek first subsection
    PDF_PROPAGATE(pdf_ctx_seek(ctx, xrefstart));
    PDF_PROPAGATE(pdf_ctx_seek_next_line(ctx));

    // Parse subsections
    do {
        LOG_DIAG(
            TRACE,
            XREF,
            "Parsing subsection %zu",
            xref_subsection_vec_len(xref->subsections)
        );

        // Parse
        uint64_t first_object;
        uint64_t num_objects;
        size_t subsection_start;

        PdfError* parse_error = pdf_xref_parse_subsection_header(
            ctx,
            &first_object,
            &num_objects,
            &subsection_start
        );
        if (parse_error) {
            LOG_DIAG(TRACE, XREF, "Bad subsection header");

            if (xref_subsection_vec_len(xref->subsections) == 0) {
                return parse_error;
            } else {
                pdf_error_free(parse_error);
            }

            break;
        }

        // Push to vec
        LOG_DIAG(
            DEBUG,
            XREF,
            "subsection=%zu, subsection_start=%lu, first_object=%lu, num_objects=%lu",
            xref_subsection_vec_len(xref->subsections),
            subsection_start,
            first_object,
            num_objects
        );

        xref_subsection_vec_push(
            xref->subsections,
            (XRefSubsection) {.start_offset = subsection_start,
                              .first_object = (uint32_t)first_object,
                              .num_entries = (uint32_t)num_objects,
                              .entries = NULL}
        );

        // Seek next subsection
        PDF_PROPAGATE(
            pdf_ctx_seek(ctx, subsection_start + 20 * num_objects - 2),
            "Failed to seek end of section. Start offset %zu, %lu objects",
            subsection_start,
            num_objects
        );

        if (!pdf_error_free_is_ok(pdf_ctx_seek_next_line(ctx))) {
            // There isn't necessarily a next line
            break;
        }
    } while (1);

    LOG_DIAG(TRACE, XREF, "Finished parsing subsection headers");

    return NULL;
}

PdfError* pdf_xref_get_entry(
    XRefTable* xref,
    size_t object_id,
    size_t generation,
    XRefEntry** entry
) {
    RELEASE_ASSERT(xref);
    RELEASE_ASSERT(entry);

    LOG_DIAG(
        DEBUG,
        XREF,
        "Getting xref object %zu with generation %zu",
        object_id,
        generation
    );

    for (size_t subsection_idx = 0;
         subsection_idx < xref_subsection_vec_len(xref->subsections);
         subsection_idx++) {
        XRefSubsection* subsection;
        RELEASE_ASSERT(xref_subsection_vec_get_ptr(
            xref->subsections,
            subsection_idx,
            &subsection
        ));

        if (object_id < subsection->first_object
            || object_id
                   >= subsection->first_object + subsection->num_entries) {
            continue;
        }

        size_t entry_idx = object_id - subsection->first_object;
        if (!subsection->entries // ASan fails at this if statement
            || !subsection->entries[entry_idx].entry_parsed) {
            LOG_DIAG(
                TRACE,
                XREF,
                "Parsing xref object %zu in subsection %zu",
                object_id,
                subsection_idx
            );
            PDF_PROPAGATE(pdf_xref_parse_entry(
                xref->arena,
                xref->ctx,
                subsection,
                entry_idx
            ));
        }

        *entry = &subsection->entries[entry_idx];

        if ((size_t)(*entry)->generation != generation) {
            return PDF_ERROR(PDF_ERR_XREF_GENERATION_MISMATCH);
        }

        return NULL;
    }

    return PDF_ERROR(PDF_ERR_INVALID_XREF_REFERENCE);
}

#ifdef TEST
#include "test/test.h"

TEST_FUNC(test_xref_create) {
    uint8_t buffer[] =
        "xref\n0 2\n0000000000 65536 f \n0000000042 00000 n \n2 1\n0000000542 00002 n ";
    Arena* arena = arena_new(128);
    PdfCtx* ctx =
        pdf_ctx_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    TEST_ASSERT(ctx);

    XRefTable* xref = pdf_xref_init(arena, ctx);
    TEST_PDF_REQUIRE(pdf_xref_parse_section(arena, ctx, 0, xref));

    TEST_ASSERT_EQ((size_t)2, xref_subsection_vec_len(xref->subsections));
    TEST_ASSERT(xref->subsections);

    XRefSubsection subsection_0;
    TEST_ASSERT(xref_subsection_vec_get(xref->subsections, 0, &subsection_0));
    TEST_ASSERT_EQ((size_t)9, subsection_0.start_offset);
    TEST_ASSERT_EQ((size_t)0, subsection_0.first_object);
    TEST_ASSERT_EQ((size_t)2, subsection_0.num_entries);

    XRefSubsection subsection_1;
    TEST_ASSERT(xref_subsection_vec_get(xref->subsections, 1, &subsection_1));
    TEST_ASSERT_EQ((size_t)53, subsection_1.start_offset);
    TEST_ASSERT_EQ((size_t)2, subsection_1.first_object);
    TEST_ASSERT_EQ((size_t)1, subsection_1.num_entries);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_xref_get_entries_ok) {
    uint8_t buffer[] =
        "xref\n0 2\n0000000000 65536 f \n0000000042 00000 n \n2 1\n0000000542 00002 n ";
    Arena* arena = arena_new(128);
    PdfCtx* ctx =
        pdf_ctx_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    TEST_ASSERT(ctx);

    XRefTable* xref = pdf_xref_init(arena, ctx);
    TEST_PDF_REQUIRE(pdf_xref_parse_section(arena, ctx, 0, xref));

    XRefEntry* entry;
    TEST_PDF_REQUIRE(pdf_xref_get_entry(xref, 0, 65536, &entry));
    TEST_ASSERT_EQ((size_t)0, entry->offset);

    TEST_PDF_REQUIRE(pdf_xref_get_entry(xref, 2, 2, &entry));
    TEST_ASSERT_EQ((size_t)542, entry->offset);

    TEST_PDF_REQUIRE(pdf_xref_get_entry(xref, 1, 0, &entry));
    TEST_ASSERT_EQ((size_t)42, entry->offset);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_xref_out_of_bound_entry) {
    uint8_t buffer[] =
        "xref\n0 2\n0000000000 65536 f \n0000000042 00000 n \n2 1\n0000000542 00002 n ";
    Arena* arena = arena_new(128);
    PdfCtx* ctx =
        pdf_ctx_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    TEST_ASSERT(ctx);

    XRefTable* xref = pdf_xref_init(arena, ctx);
    TEST_PDF_REQUIRE(pdf_xref_parse_section(arena, ctx, 0, xref));

    XRefEntry* entry;
    TEST_PDF_REQUIRE_ERR(
        pdf_xref_get_entry(xref, 3, 0, &entry),
        PDF_ERR_INVALID_XREF_REFERENCE
    );

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_xref_generation_mismatch) {
    uint8_t buffer[] =
        "xref\n0 2\n0000000000 65536 f \n0000000042 00000 n \n2 1\n0000000542 00002 n ";
    Arena* arena = arena_new(128);
    PdfCtx* ctx =
        pdf_ctx_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    TEST_ASSERT(ctx);

    XRefTable* xref = pdf_xref_init(arena, ctx);
    TEST_PDF_REQUIRE(pdf_xref_parse_section(arena, ctx, 0, xref));

    XRefEntry* entry;
    TEST_PDF_REQUIRE_ERR(
        pdf_xref_get_entry(xref, 0, 0, &entry),
        PDF_ERR_XREF_GENERATION_MISMATCH
    );

    arena_free(arena);
    return TEST_RESULT_PASS;
}

#endif // TEST
