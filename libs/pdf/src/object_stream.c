#include "object_stream.h"

#include <string.h>

#include "ctx.h"
#include "err/error.h"
#include "logger/log.h"
#include "object.h"
#include "pdf/deserde.h"
#include "pdf/types.h"
#include "resolver.h"

typedef struct {
    /// The type of PDF object that this dictionary describes; shall be ObjStm
    /// for an object stream.
    PdfName type;

    /// The number of indirect objects stored in the stream.
    PdfInteger n;

    /// The byte offset in the decoded stream of the first compressed object.
    PdfInteger first;

    /// A reference to another object stream, of which the current object
    /// stream shall be considered an extension.
    PdfIgnored extends;
} PdfObjectStreamDict;

static Error* pdf_object_stream_deserde_dict(
    const PdfObject* object,
    PdfObjectStreamDict* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    PdfFieldDescriptor fields[] = {
        pdf_name_field("Type", &target_ptr->type),
        pdf_integer_field("N", &target_ptr->n),
        pdf_integer_field("First", &target_ptr->first),
        pdf_ignored_field("Extends", &target_ptr->extends)
    };

    TRY(pdf_deserde_fields(
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        true,
        resolver,
        "PdfObjectStreamDict"
    ));

    if (strcmp(target_ptr->type, "ObjStm") != 0) {
        return ERROR(PDF_ERR_INVALID_SUBTYPE, "`Type` must be `ObjStm`");
    }

    return NULL;
}

static Error* pdf_object_stream_parse(
    PdfResolver* resolver,
    PdfCtx* stream_ctx,
    const PdfObjectStreamDict* stream_dict,
    size_t stream_index,
    size_t object_id,
    PdfObject* object
) {
    uint64_t pair_object_id = 0;
    uint64_t pair_offset = 0;
    for (size_t idx = 0; idx <= stream_index; idx++) {
        uint32_t int_length;
        TRY(pdf_ctx_consume_whitespace(stream_ctx));
        TRY(pdf_ctx_parse_int(stream_ctx, NULL, &pair_object_id, &int_length));
        if (int_length == 0) {
            return ERROR(
                PDF_ERR_INVALID_OBJECT_STREAM,
                "Expected an object number in object stream pair %zu",
                idx
            );
        }

        TRY(pdf_ctx_consume_whitespace(stream_ctx));
        TRY(pdf_ctx_parse_int(stream_ctx, NULL, &pair_offset, &int_length));
        if (int_length == 0) {
            return ERROR(
                PDF_ERR_INVALID_OBJECT_STREAM,
                "Expected a byte offset in object stream pair %zu",
                idx
            );
        }
    }

    if (pair_object_id != object_id) {
        return ERROR(
            PDF_ERR_INVALID_OBJECT_STREAM,
            "Expected object %zu at index %zu, found object %lu",
            object_id,
            stream_index,
            pair_object_id
        );
    }

    TRY(pdf_ctx_seek(
        stream_ctx,
        (size_t)stream_dict->first + (size_t)pair_offset
    ));
    TRY(pdf_parse_object(resolver, object, false));

    return NULL;
}

Error* pdf_object_stream_get(
    PdfResolver* resolver,
    const PdfObject* stream_object,
    size_t stream_index,
    size_t object_id,
    PdfObject* object
) {
    RELEASE_ASSERT(resolver);
    RELEASE_ASSERT(stream_object);
    RELEASE_ASSERT(object);

    PdfStream stream;
    TRY(pdf_deserde_stream(stream_object, &stream, resolver));

    PdfObjectStreamDict stream_dict;
    TRY(pdf_object_stream_deserde_dict(stream_object, &stream_dict, resolver));

    if (stream_dict.n < 0 || stream_dict.first < 0) {
        return ERROR(
            PDF_ERR_INVALID_OBJECT_STREAM,
            "`N` and `First` must not be negative"
        );
    }

    if (stream_index >= (size_t)stream_dict.n) {
        return ERROR(
            PDF_ERR_INVALID_OBJECT_STREAM,
            "Object index %zu out of bounds (`N` is %d)",
            stream_index,
            stream_dict.n
        );
    }

    PdfCtx* stream_ctx = pdf_ctx_new(
        pdf_resolver_arena(resolver),
        stream.stream_bytes,
        stream.decoded_stream_len
    );

    PdfCtx* file_ctx = pdf_resolver_swap_ctx(resolver, stream_ctx);
    Error* parse_error = pdf_object_stream_parse(
        resolver,
        stream_ctx,
        &stream_dict,
        stream_index,
        object_id,
        object
    );
    pdf_resolver_swap_ctx(resolver, file_ctx);
    TRY(parse_error);

    return NULL;
}

#ifdef TEST
#include "test/test.h"
#include "test_helpers.h"

TEST_FUNC(test_object_stream_get) {
    uint8_t buffer[] =
        "1 0 obj\n<</Type/ObjStm/N 2/First 9/Length 21>>\nstream\n"
        "2 0 3 9 \n<</A 5>> 42 \nendstream\nendobj";
    Arena* arena = arena_new(128);
    PdfCtx* ctx =
        pdf_ctx_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    TEST_ASSERT(ctx);

    PdfResolver* resolver = pdf_fake_resolver_new(arena, ctx);

    PdfObject stream_object;
    TEST_REQUIRE(pdf_parse_object(resolver, &stream_object, false));

    PdfObject object;
    TEST_REQUIRE(
        pdf_object_stream_get(resolver, &stream_object, 0, 2, &object)
    );
    TEST_ASSERT(object.type == PDF_OBJECT_TYPE_DICT);

    TEST_REQUIRE(
        pdf_object_stream_get(resolver, &stream_object, 1, 3, &object)
    );
    TEST_ASSERT(object.type == PDF_OBJECT_TYPE_INTEGER);
    TEST_ASSERT_EQ(42, object.data.integer);

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_stream_get_invalid) {
    uint8_t buffer[] =
        "1 0 obj\n<</Type/ObjStm/N 2/First 9/Length 21>>\nstream\n"
        "2 0 3 9 \n<</A 5>> 42 \nendstream\nendobj";
    Arena* arena = arena_new(128);
    PdfCtx* ctx =
        pdf_ctx_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1);
    TEST_ASSERT(ctx);

    PdfResolver* resolver = pdf_fake_resolver_new(arena, ctx);

    PdfObject stream_object;
    TEST_REQUIRE(pdf_parse_object(resolver, &stream_object, false));

    PdfObject object;
    TEST_REQUIRE_ERR(
        pdf_object_stream_get(resolver, &stream_object, 2, 4, &object),
        PDF_ERR_INVALID_OBJECT_STREAM
    );
    TEST_REQUIRE_ERR(
        pdf_object_stream_get(resolver, &stream_object, 0, 3, &object),
        PDF_ERR_INVALID_OBJECT_STREAM
    );

    arena_free(arena);
    return TEST_RESULT_PASS;
}

#endif // TEST
