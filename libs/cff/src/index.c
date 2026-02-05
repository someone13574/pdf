#include "index.h"

#include <stdint.h>

#include "err/error.h"
#include "logger/log.h"
#include "parse_ctx/binary.h"
#include "parse_ctx/ctx.h"
#include "test/test.h"
#include "types.h"

Error* cff_parse_index(ParseCtx* ctx, CffIndex* index_out) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(index_out);

    index_out->parser_start_offset = ctx->offset;

    TRY(parse_ctx_read_u16_be(ctx, &index_out->count));
    if (index_out->count == 0) {
        index_out->offset_size = 0;
        return NULL;
    }

    TRY(cff_read_offset_size(ctx, &index_out->offset_size));
    TRY(cff_index_skip(index_out, ctx));

    return NULL;
}

static Error* cff_index_get_object_offset(
    CffIndex* index,
    ParseCtx* ctx,
    uint16_t object_idx,
    size_t* offset_out
) {
    RELEASE_ASSERT(index);
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(offset_out);

    RELEASE_ASSERT(index->count != 0);
    RELEASE_ASSERT(
        object_idx <= index->count
    ); // Caller should do stricter check

    // Read object offset
    size_t offset_offset = index->parser_start_offset + 3
                         + (size_t)object_idx * index->offset_size;
    TRY(parse_ctx_seek(ctx, offset_offset));

    CffOffset object_rel_offset;
    TRY(cff_read_offset(ctx, index->offset_size, &object_rel_offset));

    // Get object absolute offset
    *offset_out = index->parser_start_offset + 2
                + (size_t)(index->count + 1) * index->offset_size
                + object_rel_offset;

    return NULL;
}

Error* cff_index_skip(CffIndex* index, ParseCtx* ctx) {
    RELEASE_ASSERT(index);
    RELEASE_ASSERT(ctx);

    if (index->count == 0) {
        TRY(parse_ctx_seek(ctx, index->parser_start_offset + 2));
        return NULL;
    }

    size_t end_offset;
    TRY(cff_index_get_object_offset(index, ctx, index->count, &end_offset));
    TRY(parse_ctx_seek(ctx, end_offset));

    return NULL;
}

Error* cff_index_seek_object(
    CffIndex* index,
    ParseCtx* ctx,
    uint16_t object_idx,
    size_t* object_size_out
) {
    RELEASE_ASSERT(index);
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(object_size_out);

    if (object_idx >= index->count) {
        return ERROR(
            CFF_ERR_INVALID_OBJECT_IDX,
            "Cannot seek object %u in index of %u objects",
            (unsigned int)object_idx,
            (unsigned int)index->count
        );
    }

    size_t start_offset;
    size_t end_offset;
    TRY(cff_index_get_object_offset(index, ctx, object_idx, &start_offset));
    TRY(cff_index_get_object_offset(index, ctx, object_idx + 1, &end_offset));

    if (end_offset < start_offset) {
        return ERROR(CFF_ERR_INVALID_INDEX, "Objects in INDEX not in order");
    }

    TRY(parse_ctx_seek(ctx, start_offset));

    *object_size_out = end_offset - start_offset;

    return NULL;
}

#ifdef TEST

TEST_FUNC(test_cff_index_empty_skip) {
    uint8_t buffer[] = {0x00, 0x00};
    ParseCtx ctx = parse_ctx_new(buffer, sizeof(buffer) / sizeof(uint8_t));

    CffIndex index;
    TEST_REQUIRE(cff_parse_index(&ctx, &index));

    TEST_REQUIRE(cff_index_skip(&index, &ctx));
    TEST_ASSERT_EQ((size_t)2, ctx.offset);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_index_empty_seek_object) {
    uint8_t buffer[] = {0x00, 0x00};
    ParseCtx ctx = parse_ctx_new(buffer, sizeof(buffer) / sizeof(uint8_t));

    CffIndex index;
    TEST_REQUIRE(cff_parse_index(&ctx, &index));

    size_t object_size;
    TEST_REQUIRE_ERR(
        cff_index_seek_object(&index, &ctx, 0, &object_size),
        CFF_ERR_INVALID_OBJECT_IDX
    );

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_index_seek_offset_size_1) {
    uint8_t buffer[] = {
        // size = 3
        0x00,
        0x03,
        // offset size = 1
        0x01,
        // offsets = [1, 4, 6, 7]
        0x01,
        0x04,
        0x06,
        0x07,
        // data
        'a',
        'b',
        'c',
        4,
        2,
        '@'
    };
    ParseCtx ctx = parse_ctx_new(buffer, sizeof(buffer) / sizeof(uint8_t));

    CffIndex index;
    TEST_REQUIRE(cff_parse_index(&ctx, &index));

    size_t object_size;
    TEST_REQUIRE(cff_index_seek_object(&index, &ctx, 0, &object_size));
    TEST_ASSERT_EQ((size_t)7, ctx.offset);
    TEST_ASSERT_EQ((size_t)3, object_size);

    TEST_REQUIRE(cff_index_seek_object(&index, &ctx, 1, &object_size));
    TEST_ASSERT_EQ((size_t)10, ctx.offset);
    TEST_ASSERT_EQ((size_t)2, object_size);

    TEST_REQUIRE(cff_index_seek_object(&index, &ctx, 2, &object_size));
    TEST_ASSERT_EQ((size_t)12, ctx.offset);
    TEST_ASSERT_EQ((size_t)1, object_size);

    TEST_REQUIRE_ERR(
        cff_index_seek_object(&index, &ctx, 4, &object_size),
        CFF_ERR_INVALID_OBJECT_IDX
    );

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_index_seek_offset_size_2) {
    uint8_t buffer[] = {
        // size = 3
        0x00,
        0x03,
        // offset size = 2
        0x02,
        // offsets = [1, 4, 6, 7]
        0x00,
        0x01,
        0x00,
        0x04,
        0x00,
        0x06,
        0x00,
        0x07,
        // data
        'a',
        'b',
        'c',
        4,
        2,
        '@'
    };
    ParseCtx ctx = parse_ctx_new(buffer, sizeof(buffer) / sizeof(uint8_t));

    CffIndex index;
    TEST_REQUIRE(cff_parse_index(&ctx, &index));

    size_t object_size;
    TEST_REQUIRE(cff_index_seek_object(&index, &ctx, 0, &object_size));
    TEST_ASSERT_EQ((size_t)11, ctx.offset);
    TEST_ASSERT_EQ((size_t)3, object_size);

    TEST_REQUIRE(cff_index_seek_object(&index, &ctx, 1, &object_size));
    TEST_ASSERT_EQ((size_t)14, ctx.offset);
    TEST_ASSERT_EQ((size_t)2, object_size);

    TEST_REQUIRE(cff_index_seek_object(&index, &ctx, 2, &object_size));
    TEST_ASSERT_EQ((size_t)16, ctx.offset);
    TEST_ASSERT_EQ((size_t)1, object_size);

    TEST_REQUIRE_ERR(
        cff_index_seek_object(&index, &ctx, 4, &object_size),
        CFF_ERR_INVALID_OBJECT_IDX
    );

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_index_skip_size_1) {
    uint8_t buffer[] = {
        // size = 3
        0x00,
        0x03,
        // offset size = 1
        0x01,
        // offsets = [1, 4, 6, 7]
        0x01,
        0x04,
        0x06,
        0x07,
        // data
        'a',
        'b',
        'c',
        4,
        2,
        '@'
    };
    ParseCtx ctx = parse_ctx_new(buffer, sizeof(buffer) / sizeof(uint8_t));

    CffIndex index;
    TEST_REQUIRE(cff_parse_index(&ctx, &index));

    TEST_REQUIRE(cff_index_skip(&index, &ctx));
    TEST_ASSERT_EQ((size_t)13, ctx.offset);

    return TEST_RESULT_PASS;
}

#endif // TEST
