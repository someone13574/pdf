#include "xref.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena/arena.h"
#include "ctx.h"
#include "err/error.h"
#include "logger/log.h"
#include "object.h"
#include "pdf/deserde.h"
#include "pdf/types.h"
#include "resolver.h"

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
Error* pdf_xref_parse_subsection_header(
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
    TRY(pdf_ctx_parse_int(ctx, NULL, first_object, &int_length));
    if (int_length == 0) {
        return ERROR(
            PDF_ERR_INVALID_XREF,
            "Expected an integer denoting the object number of the first object"
        );
    }

    TRY(pdf_ctx_expect(ctx, " "));

    // Parse length
    TRY(pdf_ctx_parse_int(ctx, NULL, num_objects, &int_length));
    if (int_length == 0) {
        return ERROR(
            PDF_ERR_INVALID_XREF,
            "Expected an integer denoting the subsection length"
        );
    }

    // Find start of subsection
    TRY(pdf_ctx_seek_next_line(ctx));
    *subsection_start = pdf_ctx_offset(ctx);

    return NULL;
}

Error* pdf_xref_parse_entry(
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
    TRY(pdf_ctx_seek(ctx, entry_offset));

    // Check if start of line
    TRY(pdf_ctx_seek_line_start(ctx));
    if (pdf_ctx_offset(ctx) != entry_offset) {
        return ERROR(
            PDF_ERR_INVALID_XREF,
            "XRef entry not aligned to line start"
        );
    }

    // Parse offset
    uint64_t offset;
    uint32_t expected_length = 10;
    TRY(pdf_ctx_parse_int(ctx, &expected_length, &offset, NULL));
    TRY(pdf_ctx_expect(ctx, " "));

    // Parse generation
    uint64_t generation;
    expected_length = 5;
    TRY(pdf_ctx_parse_int(ctx, &expected_length, &generation, NULL));
    TRY(pdf_ctx_expect(ctx, " "));

    uint8_t keyword;
    TRY(pdf_ctx_peek(ctx, &keyword));
    if (keyword == 'n') {
        subsection->entries[entry].type = XREF_ENTRY_IN_USE;
    } else if (keyword == 'f') {
        subsection->entries[entry].type = XREF_ENTRY_FREE;
    } else {
        return ERROR(
            PDF_ERR_INVALID_XREF,
            "Expected `n` or `f` keyword in xref entry"
        );
    }

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

Error* pdf_xref_parse_table_section(
    PdfResolver* resolver,
    size_t xrefstart,
    XRefTable* xref,
    PdfObject* trailer_dict
) {
    RELEASE_ASSERT(resolver);
    RELEASE_ASSERT(xref);
    RELEASE_ASSERT(trailer_dict);

    PdfCtx* ctx = pdf_resolver_ctx(resolver);

    // Seek first subsection
    TRY(pdf_ctx_seek(ctx, xrefstart));
    TRY(pdf_ctx_seek_next_line(ctx));

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
        size_t header_start = pdf_ctx_offset(ctx);

        Error* parse_error = pdf_xref_parse_subsection_header(
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
                error_free(parse_error);
            }

            TRY(pdf_ctx_seek(ctx, header_start));
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
        TRY(pdf_ctx_seek(ctx, subsection_start + 20 * num_objects - 2),
            "Failed to seek end of section. Start offset %zu, %lu objects",
            subsection_start,
            num_objects);

        if (!error_free_is_ok(pdf_ctx_seek_next_line(ctx))) {
            // There isn't necessarily a next line
            break;
        }
    } while (1);

    LOG_DIAG(TRACE, XREF, "Finished parsing subsection headers");

    TRY(pdf_ctx_consume_whitespace(ctx));
    TRY(pdf_ctx_expect(ctx, "trailer"));
    TRY(pdf_ctx_seek_next_line(ctx));
    TRY(pdf_parse_object(resolver, trailer_dict, false));

    return NULL;
}

typedef struct {
    /// The type of PDF object that this dictionary describes; shall be XRef
    /// for a cross-reference stream.
    PdfName type;

    /// The number one greater than the highest object number used in this
    /// section or in any section for which this shall be an update. It shall
    /// be equivalent to the Size entry in a trailer dictionary.
    PdfInteger size;

    /// An array containing a pair of integers for each subsection in this
    /// section. The first integer shall be the first object number in the
    /// subsection; the second integer shall be the number of entries in the
    /// subsection. Default value: [0 Size].
    PdfIntegerVecOptional index;

    /// Present only if the file has more than one cross-reference stream; not
    /// meaningful in hybrid-reference files. The byte offset in the decoded
    /// stream from the beginning of the file to the beginning of the previous
    /// cross-reference stream. This entry has the same function as the Prev
    /// entry in the trailer dictionary.
    PdfIgnored prev;

    /// An array of integers representing the size of the fields in a single
    /// cross-reference entry.
    PdfInteger w[3];
} XRefStreamDict;

Error* pdf_xref_deserde_stream_dict(
    const PdfObject* object,
    XRefStreamDict* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    PdfFieldDescriptor fields[] = {
        pdf_name_field("Type", &target_ptr->type),
        pdf_integer_field("Size", &target_ptr->size),
        pdf_integer_vec_optional_field("Index", &target_ptr->index),
        // Prev is deserialized as part of the trailer in pdf.c
        pdf_ignored_field("Prev", &target_ptr->prev),
        pdf_integer_fixed_array_field("W", target_ptr->w, 3, NULL)
    };

    TRY(pdf_deserde_fields(
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        true,
        resolver,
        "XRefStreamDict"
    ));

    if (strcmp(target_ptr->type, "XRef") != 0) {
        return ERROR(PDF_ERR_INVALID_SUBTYPE, "`Type` must be `XRef`");
    }

    return NULL;
}

/// Fields requiring more than one byte are stored with the high-order byte
/// first.
static uint64_t pdf_xref_read_stream_field(const uint8_t* bytes, size_t width) {
    uint64_t value = 0;
    for (size_t idx = 0; idx < width; idx++) {
        value = (value << 8) | bytes[idx];
    }
    return value;
}

static void pdf_xref_parse_stream_entry(
    const XRefStreamDict* stream_dict,
    const uint8_t* bytes,
    XRefEntry* entry
) {
    size_t w0 = (size_t)stream_dict->w[0];
    size_t w1 = (size_t)stream_dict->w[1];
    size_t w2 = (size_t)stream_dict->w[2];

    // If the first element is zero, the type field shall not be present, and
    // shall default to type 1
    uint64_t type = w0 ? pdf_xref_read_stream_field(bytes, w0) : 1;
    uint64_t field_2 = pdf_xref_read_stream_field(bytes + w0, w1);
    uint64_t field_3 = pdf_xref_read_stream_field(bytes + w0 + w1, w2);

    entry->entry_parsed = true;
    entry->object = NULL;
    entry->offset = 0;
    entry->generation = 0;
    entry->stream_object_id = 0;
    entry->stream_index = 0;

    switch (type) {
        case 0: {
            entry->type = XREF_ENTRY_FREE;
            entry->offset = (size_t)field_2;
            entry->generation = (size_t)field_3;
            break;
        }
        case 1: {
            entry->type = XREF_ENTRY_IN_USE;
            entry->offset = (size_t)field_2;
            entry->generation = (size_t)field_3;
            break;
        }
        case 2: {
            entry->type = XREF_ENTRY_COMPRESSED;
            entry->stream_object_id = (size_t)field_2;
            entry->stream_index = (size_t)field_3;
            break;
        }
        default: {
            // Any other value shall be interpreted as a reference to the null
            // object
            entry->type = XREF_ENTRY_FREE;
            break;
        }
    }
}

Error* pdf_xref_parse_stream_section(
    PdfResolver* resolver,
    XRefTable* xref,
    PdfObject* trailer_dict
) {
    RELEASE_ASSERT(resolver);
    RELEASE_ASSERT(xref);
    RELEASE_ASSERT(trailer_dict);

    PdfObject object;
    TRY(pdf_parse_object(resolver, &object, false));

    if (object.type != PDF_OBJECT_TYPE_INDIRECT_OBJECT
        || object.data.indirect_object.object->type != PDF_OBJECT_TYPE_STREAM) {
        return ERROR(
            PDF_ERR_INVALID_XREF,
            "A cross-reference stream must be an indirect stream object"
        );
    }

    PdfObject* stream_object = object.data.indirect_object.object;
    PdfStream stream = stream_object->data.stream;

    XRefStreamDict stream_dict;
    TRY(pdf_xref_deserde_stream_dict(stream_object, &stream_dict, resolver));

    size_t entry_width = 0;
    for (size_t idx = 0; idx < 3; idx++) {
        if (stream_dict.w[idx] < 0
            || (size_t)stream_dict.w[idx] > sizeof(uint64_t)) {
            return ERROR(
                PDF_ERR_INVALID_XREF,
                "Invalid field width %d in `W`",
                stream_dict.w[idx]
            );
        }
        entry_width += (size_t)stream_dict.w[idx];
    }

    // Field 2 has no default value for any entry type
    if (stream_dict.w[1] == 0) {
        return ERROR(PDF_ERR_INVALID_XREF, "Field 2 of `W` must not be zero");
    }

    PdfIntegerVec* index;
    if (stream_dict.index.is_some) {
        index = stream_dict.index.value;
        if (pdf_integer_vec_len(index) % 2 != 0) {
            return ERROR(
                PDF_ERR_INVALID_XREF,
                "`Index` must contain a pair of integers for each subsection"
            );
        }
    } else {
        // Default value: [0 Size]
        index = pdf_integer_vec_new(pdf_resolver_arena(resolver));
        pdf_integer_vec_push(index, 0);
        pdf_integer_vec_push(index, stream_dict.size);
    }

    const uint8_t* bytes = stream.stream_bytes;
    size_t entries_remaining = stream.decoded_stream_len / entry_width;

    for (size_t pair_idx = 0; pair_idx < pdf_integer_vec_len(index);
         pair_idx += 2) {
        PdfInteger first_object;
        PdfInteger num_entries;
        RELEASE_ASSERT(pdf_integer_vec_get(index, pair_idx, &first_object));
        RELEASE_ASSERT(pdf_integer_vec_get(index, pair_idx + 1, &num_entries));

        if (first_object < 0 || num_entries < 0) {
            return ERROR(
                PDF_ERR_INVALID_XREF,
                "`Index` integers must not be negative"
            );
        }

        if ((size_t)num_entries > entries_remaining) {
            return ERROR(
                PDF_ERR_INVALID_XREF,
                "Cross-reference stream is too short for its `Index`"
            );
        }

        if (num_entries == 0) {
            continue;
        }

        XRefEntry* entries = arena_alloc(
            pdf_resolver_arena(resolver),
            sizeof(XRefEntry) * (size_t)num_entries
        );
        for (size_t idx = 0; idx < (size_t)num_entries; idx++) {
            pdf_xref_parse_stream_entry(
                &stream_dict,
                bytes + idx * entry_width,
                &entries[idx]
            );
        }

        LOG_DIAG(
            DEBUG,
            XREF,
            "stream subsection=%zu, first_object=%d, num_objects=%d",
            xref_subsection_vec_len(xref->subsections),
            first_object,
            num_entries
        );

        xref_subsection_vec_push(
            xref->subsections,
            (XRefSubsection) {.start_offset = 0,
                              .first_object = (size_t)first_object,
                              .num_entries = (size_t)num_entries,
                              .entries = entries}
        );

        bytes += (size_t)num_entries * entry_width;
        entries_remaining -= (size_t)num_entries;
    }

    *trailer_dict = *stream_object;

    return NULL;
}

Error* pdf_xref_parse_section(
    PdfResolver* resolver,
    size_t xrefstart,
    XRefTable* xref,
    PdfObject* trailer_dict
) {
    RELEASE_ASSERT(resolver);
    RELEASE_ASSERT(xref);
    RELEASE_ASSERT(trailer_dict);

    PdfCtx* ctx = pdf_resolver_ctx(resolver);

    TRY(pdf_ctx_seek(ctx, xrefstart));
    Error* keyword_error = pdf_ctx_expect(ctx, "xref");
    if (!keyword_error) {
        return pdf_xref_parse_table_section(
            resolver,
            xrefstart,
            xref,
            trailer_dict
        );
    }
    error_free(keyword_error);

    TRY(pdf_ctx_seek(ctx, xrefstart));
    return pdf_xref_parse_stream_section(resolver, xref, trailer_dict);
}

Error* pdf_xref_get_entry(
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
            TRY(pdf_xref_parse_entry(
                xref->arena,
                xref->ctx,
                subsection,
                entry_idx
            ));
        }

        *entry = &subsection->entries[entry_idx];

        if ((size_t)(*entry)->generation != generation) {
            return ERROR(PDF_ERR_XREF_GENERATION_MISMATCH);
        }

        return NULL;
    }

    return ERROR(PDF_ERR_INVALID_XREF_REFERENCE);
}

#ifdef TEST
#include "test/test.h"
#include "test_helpers.h"

TEST_FUNC(test_xref_create) {
    uint8_t buffer[] =
        "xref\n0 2\n0000000000 65536 f \n0000000042 00000 n \n2 1\n0000000542 00002 n \ntrailer\n<</Size 3>>";
    Arena* arena = arena_new(128);
    PdfCtx* ctx =
        pdf_ctx_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    TEST_ASSERT(ctx);

    PdfResolver* resolver = pdf_fake_resolver_new(arena, ctx);
    XRefTable* xref = pdf_xref_init(arena, ctx);

    PdfObject trailer_dict;
    TEST_REQUIRE(pdf_xref_parse_section(resolver, 0, xref, &trailer_dict));
    TEST_ASSERT(trailer_dict.type == PDF_OBJECT_TYPE_DICT);

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
        "xref\n0 2\n0000000000 65536 f \n0000000042 00000 n \n2 1\n0000000542 00002 n \ntrailer\n<</Size 3>>";
    Arena* arena = arena_new(128);
    PdfCtx* ctx =
        pdf_ctx_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    TEST_ASSERT(ctx);

    PdfResolver* resolver = pdf_fake_resolver_new(arena, ctx);
    XRefTable* xref = pdf_xref_init(arena, ctx);

    PdfObject trailer_dict;
    TEST_REQUIRE(pdf_xref_parse_section(resolver, 0, xref, &trailer_dict));

    XRefEntry* entry;
    TEST_REQUIRE(pdf_xref_get_entry(xref, 0, 65536, &entry));
    TEST_ASSERT_EQ((size_t)0, entry->offset);

    TEST_REQUIRE(pdf_xref_get_entry(xref, 2, 2, &entry));
    TEST_ASSERT_EQ((size_t)542, entry->offset);

    TEST_REQUIRE(pdf_xref_get_entry(xref, 1, 0, &entry));
    TEST_ASSERT_EQ((size_t)42, entry->offset);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_xref_out_of_bound_entry) {
    uint8_t buffer[] =
        "xref\n0 2\n0000000000 65536 f \n0000000042 00000 n \n2 1\n0000000542 00002 n \ntrailer\n<</Size 3>>";
    Arena* arena = arena_new(128);
    PdfCtx* ctx =
        pdf_ctx_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    TEST_ASSERT(ctx);

    PdfResolver* resolver = pdf_fake_resolver_new(arena, ctx);
    XRefTable* xref = pdf_xref_init(arena, ctx);

    PdfObject trailer_dict;
    TEST_REQUIRE(pdf_xref_parse_section(resolver, 0, xref, &trailer_dict));

    XRefEntry* entry;
    TEST_REQUIRE_ERR(
        pdf_xref_get_entry(xref, 3, 0, &entry),
        PDF_ERR_INVALID_XREF_REFERENCE
    );

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_xref_generation_mismatch) {
    uint8_t buffer[] =
        "xref\n0 2\n0000000000 65536 f \n0000000042 00000 n \n2 1\n0000000542 00002 n \ntrailer\n<</Size 3>>";
    Arena* arena = arena_new(128);
    PdfCtx* ctx =
        pdf_ctx_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    TEST_ASSERT(ctx);

    PdfResolver* resolver = pdf_fake_resolver_new(arena, ctx);
    XRefTable* xref = pdf_xref_init(arena, ctx);

    PdfObject trailer_dict;
    TEST_REQUIRE(pdf_xref_parse_section(resolver, 0, xref, &trailer_dict));

    XRefEntry* entry;
    TEST_REQUIRE_ERR(
        pdf_xref_get_entry(xref, 0, 0, &entry),
        PDF_ERR_XREF_GENERATION_MISMATCH
    );

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_xref_stream_create) {
    uint8_t buffer[] =
        "5 0 obj\n<</Type/XRef/Size 11/Index[0 2 10 1]/W[1 2 1]/Length 12>>\nstream\n"
        "\x00\x00\x00\xff\x01\x00\x2a\x00\x02\x00\x05\x07"
        "\nendstream\nendobj";
    Arena* arena = arena_new(128);
    PdfCtx* ctx =
        pdf_ctx_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    TEST_ASSERT(ctx);

    PdfResolver* resolver = pdf_fake_resolver_new(arena, ctx);
    XRefTable* xref = pdf_xref_init(arena, ctx);

    PdfObject trailer_dict;
    TEST_REQUIRE(pdf_xref_parse_section(resolver, 0, xref, &trailer_dict));
    TEST_ASSERT(trailer_dict.type == PDF_OBJECT_TYPE_STREAM);
    TEST_ASSERT_EQ((size_t)2, xref_subsection_vec_len(xref->subsections));

    XRefEntry* entry;
    TEST_REQUIRE(pdf_xref_get_entry(xref, 0, 255, &entry));
    TEST_ASSERT(entry->type == XREF_ENTRY_FREE);

    TEST_REQUIRE(pdf_xref_get_entry(xref, 1, 0, &entry));
    TEST_ASSERT(entry->type == XREF_ENTRY_IN_USE);
    TEST_ASSERT_EQ((size_t)42, entry->offset);

    TEST_REQUIRE(pdf_xref_get_entry(xref, 10, 0, &entry));
    TEST_ASSERT(entry->type == XREF_ENTRY_COMPRESSED);
    TEST_ASSERT_EQ((size_t)5, entry->stream_object_id);
    TEST_ASSERT_EQ((size_t)7, entry->stream_index);

    TEST_REQUIRE_ERR(
        pdf_xref_get_entry(xref, 5, 0, &entry),
        PDF_ERR_INVALID_XREF_REFERENCE
    );

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_xref_stream_defaults) {
    uint8_t buffer[] =
        "5 0 obj\n<</Type/XRef/Size 2/W[0 1 1]/Length 4>>\nstream\n"
        "\x00\xff\x2a\x00"
        "\nendstream\nendobj";
    Arena* arena = arena_new(128);
    PdfCtx* ctx =
        pdf_ctx_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    TEST_ASSERT(ctx);

    PdfResolver* resolver = pdf_fake_resolver_new(arena, ctx);
    XRefTable* xref = pdf_xref_init(arena, ctx);

    PdfObject trailer_dict;
    TEST_REQUIRE(pdf_xref_parse_section(resolver, 0, xref, &trailer_dict));
    TEST_ASSERT_EQ((size_t)1, xref_subsection_vec_len(xref->subsections));

    XRefEntry* entry;
    TEST_REQUIRE(pdf_xref_get_entry(xref, 0, 255, &entry));
    TEST_ASSERT(entry->type == XREF_ENTRY_IN_USE);
    TEST_ASSERT_EQ((size_t)0, entry->offset);

    TEST_REQUIRE(pdf_xref_get_entry(xref, 1, 0, &entry));
    TEST_ASSERT(entry->type == XREF_ENTRY_IN_USE);
    TEST_ASSERT_EQ((size_t)42, entry->offset);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_xref_stream_too_short) {
    uint8_t buffer[] =
        "5 0 obj\n<</Type/XRef/Size 5/W[1 2 1]/Length 4>>\nstream\n"
        "\x01\x00\x2a\x00"
        "\nendstream\nendobj";
    Arena* arena = arena_new(128);
    PdfCtx* ctx =
        pdf_ctx_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    TEST_ASSERT(ctx);

    PdfResolver* resolver = pdf_fake_resolver_new(arena, ctx);
    XRefTable* xref = pdf_xref_init(arena, ctx);

    PdfObject trailer_dict;
    TEST_REQUIRE_ERR(
        pdf_xref_parse_section(resolver, 0, xref, &trailer_dict),
        PDF_ERR_INVALID_XREF
    );

    arena_free(arena);
    return TEST_RESULT_PASS;
}

#endif // TEST
