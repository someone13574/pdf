#include "xref.h"

#include <linux/limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "ctx.h"
#include "log.h"
#include "result.h"
#include "vec.h"

typedef struct {
    size_t offset;
    size_t generation;
    bool parsed;
} XRefEntry;

typedef struct {
    size_t start_offset;
    size_t first_object;
    size_t num_entries;
    XRefEntry* entries;
} XRefSubsection;

struct XRefTable {
    Arena* arena;
    PdfCtx* ctx;
    Vec* subsections;
};

// Each cross-reference subsection shall contain entries for a contiguous range
// of object numbers. The subsection shall begin with a line containing two
// numbers separated by a SPACE (20h), denoting the object number of the first
// object in this subsection and the number of entries in the subsection.
PdfResult pdf_xref_parse_subsection_header(
    PdfCtx* ctx,
    uint64_t* first_object,
    uint64_t* num_objects,
    size_t* subsection_start
) {
    DBG_ASSERT(ctx);
    DBG_ASSERT(first_object);
    DBG_ASSERT(num_objects);

    // Parse object index
    uint32_t int_length;
    PDF_TRY(pdf_ctx_parse_int(ctx, NULL, first_object, &int_length));
    if (int_length == 0) {
        return PDF_ERR_INVALID_XREF;
    }

    PDF_TRY(pdf_ctx_expect(ctx, " "));

    // Parse length
    PDF_TRY(pdf_ctx_parse_int(ctx, NULL, num_objects, &int_length));
    if (int_length == 0) {
        return PDF_ERR_INVALID_XREF;
    }

    // Find start of subsection
    PDF_TRY(pdf_ctx_seek_next_line(ctx));
    *subsection_start = pdf_ctx_offset(ctx);

    return PDF_OK;
}

PdfResult pdf_xref_parse_entry(
    Arena* arena,
    PdfCtx* ctx,
    XRefSubsection* subsection,
    size_t entry
) {
    DBG_ASSERT(arena);
    DBG_ASSERT(subsection);
    DBG_ASSERT(entry < subsection->num_entries);

    if (!subsection->entries) {
        LOG_DEBUG_G(
            "xref",
            "Initializing entries table for subsection with %zu entries",
            subsection->num_entries
        );

        subsection->entries =
            arena_alloc(arena, sizeof(XRefEntry) * subsection->num_entries);
        for (size_t idx = 0; idx < subsection->num_entries; idx++) {
            subsection->entries[idx].parsed = false;
        }
    }

    // Seek entry
    size_t entry_offset = subsection->start_offset + 20 * entry;
    PDF_TRY(pdf_ctx_seek(ctx, entry_offset));

    // Check if start of line
    PDF_TRY(pdf_ctx_seek_line_start(ctx));
    if (pdf_ctx_offset(ctx) != entry_offset) {
        LOG_ERROR_G("xref", "XRef entry not aligned to line start");
        return PDF_ERR_INVALID_XREF;
    }

    // Parse offset
    uint64_t offset;
    uint32_t expected_length = 10;
    PDF_TRY(pdf_ctx_parse_int(ctx, &expected_length, &offset, NULL));
    PDF_TRY(pdf_ctx_expect(ctx, " "));

    // Parse generation
    uint64_t generation;
    expected_length = 5;
    PDF_TRY(pdf_ctx_parse_int(ctx, &expected_length, &generation, NULL));

    subsection->entries[entry].offset = (size_t)offset;
    subsection->entries[entry].generation = (size_t)generation;
    subsection->entries[entry].parsed = true;

    return PDF_OK;
}

XRefTable*
pdf_xref_new(Arena* arena, PdfCtx* ctx, size_t xrefstart, PdfResult* result) {
    DBG_ASSERT(ctx);
    DBG_ASSERT(arena);

    if (result) {
        *result = PDF_OK;
    }

    XRefTable* xref = arena_alloc(arena, sizeof(XRefTable));
    LOG_ASSERT(xref, "XRefTable allocation failed");

    // Validate xrefstart
    PDF_TRY_RET_NULL(pdf_ctx_seek(ctx, xrefstart));

    PdfResult line_seek_result = pdf_ctx_seek_line_start(ctx);
    if (line_seek_result == PDF_OK && pdf_ctx_offset(ctx) != xrefstart) {
        LOG_WARN_G("parse", "xrefstart not pointing to start of line");
        if (result) {
            *result = PDF_ERR_INVALID_XREF;
        }
        return NULL;
    }

    PDF_TRY_RET_NULL(pdf_ctx_expect(ctx, "xref"));

    // Seek first subsection
    PDF_TRY_RET_NULL(pdf_ctx_seek(ctx, xrefstart));
    PDF_TRY_RET_NULL(pdf_ctx_seek_next_line(ctx));

    xref->ctx = ctx;
    xref->arena = arena;
    xref->subsections = vec_new(arena);
    LOG_ASSERT(xref->subsections, "Allocation failed");

    // Parse subsections
    do {
        LOG_TRACE_G(
            "xref",
            "Parsing subsection %zu",
            vec_len(xref->subsections)
        );

        // Parse
        uint64_t first_object;
        uint64_t num_objects;
        size_t subsection_start;

        PdfResult parse_result = pdf_xref_parse_subsection_header(
            ctx,
            &first_object,
            &num_objects,
            &subsection_start
        );
        if (parse_result != PDF_OK) {
            LOG_TRACE_G("xref", "Bad subsection header");

            if (vec_len(xref->subsections) == 0) {
                if (result) {
                    *result = parse_result;
                }

                return NULL;
            }

            break;
        }

        LOG_DEBUG_G(
            "xref",
            "subsection=%zu, subsection_start=%lu, first_object=%lu, num_objects=%lu",
            vec_len(xref->subsections),
            subsection_start,
            first_object,
            num_objects
        );

        // Allocate
        XRefSubsection* subsection = arena_alloc(arena, sizeof(XRefSubsection));
        subsection->start_offset = subsection_start;
        subsection->first_object = (uint32_t)first_object;
        subsection->num_entries = (uint32_t)num_objects;
        subsection->entries = NULL; // allocated when used

        vec_push(xref->subsections, subsection);

        // Seek next subsection
        PdfResult seek_end_result =
            pdf_ctx_seek(ctx, subsection_start + 20 * num_objects - 2);
        if (seek_end_result != PDF_OK) {
            LOG_TRACE_G(
                "xref",
                "Failed to seek end of section. Start offset %zu, %lu objects",
                subsection_start,
                num_objects
            );

            if (result) {
                *result = seek_end_result;
            }

            return NULL;
        }

        if (pdf_ctx_seek_next_line(ctx) != PDF_OK) {
            // There isn't necessarily a next line
            break;
        }
    } while (1);

    LOG_TRACE_G("xref", "Finished parsing subsection headers");

    return xref;
}

PdfResult pdf_xref_get_ref_location(
    XRefTable* xref,
    size_t object_id,
    size_t generation,
    size_t* location
) {
    DBG_ASSERT(xref);
    DBG_ASSERT(location);

    LOG_DEBUG_G(
        "xref",
        "Getting xref object %zu with generation %zu",
        object_id,
        generation
    );

    for (size_t subsection_idx = 0; subsection_idx < vec_len(xref->subsections);
         subsection_idx++) {
        XRefSubsection* subsection = vec_get(xref->subsections, subsection_idx);

        if (object_id < subsection->first_object
            || object_id
                >= subsection->first_object + subsection->num_entries) {
            continue;
        }

        size_t entry_idx = object_id - subsection->first_object;
        if (!subsection->entries || !subsection->entries[entry_idx].parsed) {
            LOG_TRACE_G(
                "xref",
                "Parsing xref object %zu in subsection %zu",
                object_id,
                subsection_idx
            );
            PDF_TRY(pdf_xref_parse_entry(
                xref->arena,
                xref->ctx,
                subsection,
                entry_idx
            ));
        }

        XRefEntry* entry = &subsection->entries[entry_idx];
        *location = entry->offset;

        if ((size_t)entry->generation != generation) {
            return PDF_ERR_XREF_GENERATION_MISMATCH;
        }

        return PDF_OK;
    }

    return PDF_ERR_INVALID_XREF_REFERENCE;
}

#ifdef TEST
#include "test.h"

TEST_FUNC(test_xref_create) {
    char buffer[] =
        "xref\n0 2\n0000000000 65536 f \n0000000042 00000 n \n2 1\n0000000542 00002 n ";
    Arena* arena = arena_new(128);
    PdfCtx* ctx = pdf_ctx_new(arena, buffer, strlen(buffer));
    TEST_ASSERT(ctx);

    PdfResult result;
    XRefTable* xref = pdf_xref_new(arena, ctx, 0, &result);
    TEST_ASSERT_EQ((PdfResult)PDF_OK, result);
    TEST_ASSERT(xref);

    TEST_ASSERT_EQ((size_t)2, vec_len(xref->subsections));
    TEST_ASSERT(xref->subsections);

    XRefSubsection* subsection_0 = vec_get(xref->subsections, 0);
    TEST_ASSERT_EQ((size_t)9, subsection_0->start_offset);
    TEST_ASSERT_EQ((size_t)0, subsection_0->first_object);
    TEST_ASSERT_EQ((size_t)2, subsection_0->num_entries);

    XRefSubsection* subsection_1 = vec_get(xref->subsections, 1);
    TEST_ASSERT_EQ((size_t)53, subsection_1->start_offset);
    TEST_ASSERT_EQ((size_t)2, subsection_1->first_object);
    TEST_ASSERT_EQ((size_t)1, subsection_1->num_entries);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_xref_get_entries_ok) {
    char buffer[] =
        "xref\n0 2\n0000000000 65536 f \n0000000042 00000 n \n2 1\n0000000542 00002 n ";
    Arena* arena = arena_new(128);
    PdfCtx* ctx = pdf_ctx_new(arena, buffer, strlen(buffer));
    TEST_ASSERT(ctx);

    PdfResult result;
    XRefTable* xref = pdf_xref_new(arena, ctx, 0, &result);
    TEST_ASSERT_EQ((PdfResult)PDF_OK, result);
    TEST_ASSERT(xref);

    size_t offset;
    TEST_ASSERT_EQ(
        (PdfResult)PDF_OK,
        pdf_xref_get_ref_location(xref, 0, 65536, &offset)
    );
    TEST_ASSERT_EQ((size_t)0, offset);

    TEST_ASSERT_EQ(
        (PdfResult)PDF_OK,
        pdf_xref_get_ref_location(xref, 2, 2, &offset)
    );
    TEST_ASSERT_EQ((size_t)542, offset);

    TEST_ASSERT_EQ(
        (PdfResult)PDF_OK,
        pdf_xref_get_ref_location(xref, 1, 0, &offset)
    );
    TEST_ASSERT_EQ((size_t)42, offset);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_xref_out_of_bound_entry) {
    char buffer[] =
        "xref\n0 2\n0000000000 65536 f \n0000000042 00000 n \n2 1\n0000000542 00002 n ";
    Arena* arena = arena_new(128);
    PdfCtx* ctx = pdf_ctx_new(arena, buffer, strlen(buffer));
    TEST_ASSERT(ctx);

    PdfResult result;
    XRefTable* xref = pdf_xref_new(arena, ctx, 0, &result);
    TEST_ASSERT_EQ((PdfResult)PDF_OK, result);
    TEST_ASSERT(xref);

    size_t offset;
    TEST_ASSERT_EQ(
        (PdfResult)PDF_ERR_INVALID_XREF_REFERENCE,
        pdf_xref_get_ref_location(xref, 3, 0, &offset)
    );

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_xref_generation_mismatch) {
    char buffer[] =
        "xref\n0 2\n0000000000 65536 f \n0000000042 00000 n \n2 1\n0000000542 00002 n ";
    Arena* arena = arena_new(128);
    PdfCtx* ctx = pdf_ctx_new(arena, buffer, strlen(buffer));
    TEST_ASSERT(ctx);

    PdfResult result;
    XRefTable* xref = pdf_xref_new(arena, ctx, 0, &result);
    TEST_ASSERT_EQ((PdfResult)PDF_OK, result);
    TEST_ASSERT(xref);

    size_t offset;
    TEST_ASSERT_EQ(
        (PdfResult)PDF_ERR_XREF_GENERATION_MISMATCH,
        pdf_xref_get_ref_location(xref, 0, 0, &offset)
    );

    arena_free(arena);
    return TEST_RESULT_PASS;
}

#endif // TEST
