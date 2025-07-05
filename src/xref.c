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
    uint16_t generation;
    bool parsed;
} XRefObject;

typedef struct {
    size_t start_offset;
    uint32_t first_object;
    uint32_t num_objects;
    XRefObject* objects;
} XRefSubsection;

struct XRefTable {
    PdfCtx* ctx;
    Vec* subsections;
};

// Each cross-reference subsection shall contain entries for a contiguous range
// of object numbers. The subsection shall begin with a line containing two
// numbers separated by a SPACE (20h), denoting the object number of the first
// object in this subsection and the number of entries in the subsection.
PdfResult pdf_xref_parse_subsection_header(
    PdfCtx* ctx,
    unsigned long long* first_object,
    unsigned long long* num_objects,
    size_t* subsection_start
) {
    DBG_ASSERT(ctx);
    DBG_ASSERT(first_object);
    DBG_ASSERT(num_objects);

    size_t start_of_header = pdf_ctx_offset(ctx);
    PDF_TRY(pdf_ctx_seek_next_line(ctx));
    size_t end_of_header = pdf_ctx_offset(ctx);
    DBG_ASSERT(start_of_header < end_of_header);

    long read_length;
    PDF_TRY(pdf_ctx_parse_int(
        ctx,
        start_of_header,
        end_of_header - start_of_header,
        first_object,
        &read_length
    ));

    PDF_TRY(pdf_ctx_seek(ctx, start_of_header + (size_t)read_length));
    PDF_TRY(pdf_ctx_expect(ctx, " "));

    PDF_TRY(pdf_ctx_parse_int(
        ctx,
        pdf_ctx_offset(ctx),
        end_of_header - pdf_ctx_offset(ctx),
        num_objects,
        NULL
    ));

    *subsection_start = end_of_header; // end_of_header = start of next line

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
    PdfResult seek_result = pdf_ctx_seek(ctx, xrefstart);
    if (seek_result != PDF_OK) {
        if (result) {
            *result = seek_result;
        }
        return NULL;
    }

    PdfResult line_seek_result = pdf_ctx_seek_line_start(ctx);
    if (line_seek_result == PDF_OK && pdf_ctx_offset(ctx) != xrefstart) {
        LOG_WARN_G("parse", "xrefstart not pointing to start of line");
        seek_result = PDF_ERR_INVALID_XREF;
    }

    if (seek_result != PDF_OK) {
        if (result) {
            *result = seek_result;
        }
        return NULL;
    }

    PdfResult expect_result = pdf_ctx_expect(ctx, "xref");
    if (expect_result != PDF_OK) {
        if (result) {
            *result = expect_result;
        }

        return NULL;
    }

    // Seek first subsection
    pdf_ctx_seek(ctx, xrefstart);
    PdfResult next_line_result = pdf_ctx_seek_next_line(ctx);
    if (next_line_result != PDF_OK) {
        if (result) {
            *result = next_line_result;
        }
        return NULL;
    }

    xref->ctx = ctx;
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
        unsigned long long first_object;
        unsigned long long num_objects;
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
            "subsection=%zu, subsection_start=%lu, first_object=%llu, num_objects=%llu",
            vec_len(xref->subsections),
            subsection_start,
            first_object,
            num_objects
        );

        // Allocate
        XRefSubsection* subsection = arena_alloc(arena, sizeof(XRefSubsection));
        subsection->start_offset = subsection_start;
        subsection->first_object = (uint32_t)first_object;
        subsection->num_objects = (uint32_t)num_objects;
        subsection->objects = NULL; // allocated when used

        vec_push(xref->subsections, subsection);

        // Seek next subsection
        PdfResult seek_end_result =
            pdf_ctx_seek(ctx, subsection_start + 20 * num_objects - 2);
        if (seek_end_result != PDF_OK) {
            LOG_TRACE_G(
                "xref",
                "Failed to seek end of section. Start offset %lu, %llu objects",
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
    TEST_ASSERT_EQ((uint32_t)0, subsection_0->first_object);
    TEST_ASSERT_EQ((uint32_t)2, subsection_0->num_objects);

    XRefSubsection* subsection_1 = vec_get(xref->subsections, 1);
    TEST_ASSERT_EQ((size_t)53, subsection_1->start_offset);
    TEST_ASSERT_EQ((uint32_t)2, subsection_1->first_object);
    TEST_ASSERT_EQ((uint32_t)1, subsection_1->num_objects);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

#endif // TEST
