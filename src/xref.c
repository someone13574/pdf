#include "xref.h"

#include <linux/limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ctx.h"
#include "log.h"
#include "result.h"

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

    size_t num_subsections;
    size_t subsections_capacity;
    XRefSubsection* subsections;
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

void pdf_xref_free(XRefTable** xref_ref) {
    DBG_ASSERT(xref_ref);
    XRefTable* xref = *xref_ref;

    if (xref) {
        if (xref->subsections) {
            for (size_t subsection_idx = 0;
                 subsection_idx < xref->num_subsections;
                 subsection_idx++) {
                free(xref->subsections[subsection_idx].objects);
            }
            free(xref->subsections);
            xref->subsections = NULL;
        }

        free(xref);
    }

    *xref_ref = NULL;
}

XRefTable* pdf_xref_create(PdfCtx* ctx, size_t xrefstart, PdfResult* result) {
    DBG_ASSERT(ctx);
    if (result) {
        *result = PDF_OK;
    }

    XRefTable* xref = malloc(sizeof(XRefTable));
    LOG_ASSERT(xref, "XRefTable allocation failed");

    // Validate xrefstart
    PdfResult seek_result = pdf_ctx_seek(ctx, xrefstart);
    if (seek_result != PDF_OK) {
        if (result) {
            *result = seek_result;
        }

        free(xref);
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

        free(xref);
        return NULL;
    }

    PdfResult expect_result = pdf_ctx_expect(ctx, "xref");
    if (expect_result != PDF_OK) {
        if (result) {
            *result = expect_result;
        }

        free(xref);
        return NULL;
    }

    // Seek first subsection
    pdf_ctx_seek(ctx, xrefstart);
    PdfResult next_line_result = pdf_ctx_seek_next_line(ctx);
    if (next_line_result != PDF_OK) {
        if (result) {
            *result = next_line_result;
        }

        free(xref);
        return NULL;
    }

    xref->ctx = ctx;
    xref->num_subsections = 0;
    xref->subsections_capacity = 1;
    xref->subsections =
        calloc(sizeof(XRefSubsection), xref->subsections_capacity);
    LOG_ASSERT(xref->subsections, "Allocation failed");

    // Parse subsections
    do {
        LOG_TRACE_G("xref", "Parsing subsection %lu", xref->num_subsections);

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

            if (xref->num_subsections == 0) {
                if (result) {
                    *result = parse_result;
                }

                pdf_xref_free(&xref);
                return NULL;
            }

            break;
        }

        LOG_DEBUG_G(
            "xref",
            "subsection=%lu, subsection_start=%lu, first_object=%llu, num_objects=%llu",
            xref->num_subsections,
            subsection_start,
            first_object,
            num_objects
        );

        // Allocate
        XRefSubsection subsection;
        subsection.start_offset = subsection_start;
        subsection.first_object = (uint32_t)first_object;
        subsection.num_objects = (uint32_t)num_objects;
        subsection.objects = NULL; // allocated when used

        if (++xref->num_subsections > xref->subsections_capacity) {
            size_t old_capacity = xref->subsections_capacity;
            xref->subsections_capacity *= 2;

            XRefSubsection* realloced = realloc(
                xref->subsections,
                sizeof(XRefSubsection) * xref->subsections_capacity
            );
            LOG_ASSERT(realloced, "Re-allocation failed");

            memset(
                realloced + sizeof(XRefSubsection) * old_capacity,
                0,
                sizeof(XRefSubsection)
                    * (old_capacity - xref->subsections_capacity)
            );
            xref->subsections = realloced;
        }

        xref->subsections[xref->num_subsections - 1] = subsection;

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

            pdf_xref_free(&xref);
            return NULL;
        }

        if (pdf_ctx_seek_next_line(ctx) != PDF_OK) {
            // There isn't necessarily a next line
            break;
        }
    } while (1);

    return xref;
}

#ifdef TEST
#include "test.h"

TEST_FUNC(test_xref_create) {
    char buffer[] =
        "xref\n0 2\n0000000000 65536 f \n0000000042 00000 n \n2 1\n0000000542 00002 n ";
    PdfCtx* ctx = pdf_ctx_new(buffer, strlen(buffer));
    TEST_ASSERT(ctx, "Creation failed");

    PdfResult result;
    XRefTable* xref = pdf_xref_create(ctx, 0, &result);
    TEST_ASSERT_EQ((PdfResult)PDF_OK, result);
    TEST_ASSERT(xref, "Creation failed");

    TEST_ASSERT_EQ((size_t)2, xref->num_subsections);
    TEST_ASSERT(xref->subsections, "Subsections not allocated");

    TEST_ASSERT_EQ((size_t)9, xref->subsections[0].start_offset);
    TEST_ASSERT_EQ((uint32_t)0, xref->subsections[0].first_object);
    TEST_ASSERT_EQ((uint32_t)2, xref->subsections[0].num_objects);

    TEST_ASSERT_EQ((size_t)53, xref->subsections[1].start_offset);
    TEST_ASSERT_EQ((uint32_t)2, xref->subsections[1].first_object);
    TEST_ASSERT_EQ((uint32_t)1, xref->subsections[1].num_objects);

    return TEST_SUCCESS;
}

#endif // TEST
