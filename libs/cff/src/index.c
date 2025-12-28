#include "index.h"

#include <stdint.h>

#include "logger/log.h"
#include "parser.h"
#include "pdf_error/error.h"
#include "test/test.h"
#include "types.h"

PdfError* cff_parse_index(CffParser* parser, CffIndex* index_out) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(index_out);

    index_out->parser_start_offset = parser->offset;

    PDF_PROPAGATE(cff_parser_read_card16(parser, &index_out->count));
    if (index_out->count == 0) {
        index_out->offset_size = 0;
        return NULL;
    }

    PDF_PROPAGATE(cff_parser_read_offset_size(parser, &index_out->offset_size));
    PDF_PROPAGATE(cff_index_skip(index_out, parser));

    return NULL;
}

static PdfError* cff_index_get_object_offset(
    CffIndex* index,
    CffParser* parser,
    CffCard16 object_idx,
    size_t* offset_out
) {
    RELEASE_ASSERT(index);
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(offset_out);

    RELEASE_ASSERT(index->count != 0);
    RELEASE_ASSERT(
        object_idx <= index->count
    ); // Caller should do stricter check

    // Read object offset
    size_t offset_offset = index->parser_start_offset + 3
                         + (size_t)object_idx * index->offset_size;
    PDF_PROPAGATE(cff_parser_seek(parser, offset_offset));

    CffOffset object_rel_offset;
    PDF_PROPAGATE(
        cff_parser_read_offset(parser, index->offset_size, &object_rel_offset)
    );

    // Get object absolute offset
    *offset_out = index->parser_start_offset + 2
                + (size_t)(index->count + 1) * index->offset_size
                + object_rel_offset;

    return NULL;
}

PdfError* cff_index_skip(CffIndex* index, CffParser* parser) {
    RELEASE_ASSERT(index);
    RELEASE_ASSERT(parser);

    if (index->count == 0) {
        PDF_PROPAGATE(cff_parser_seek(parser, index->parser_start_offset + 2));
        return NULL;
    }

    size_t end_offset;
    PDF_PROPAGATE(
        cff_index_get_object_offset(index, parser, index->count, &end_offset)
    );
    PDF_PROPAGATE(cff_parser_seek(parser, end_offset));

    return NULL;
}

PdfError* cff_index_seek_object(
    CffIndex* index,
    CffParser* parser,
    CffCard16 object_idx,
    size_t* object_size_out
) {
    RELEASE_ASSERT(index);
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(object_size_out);

    if (object_idx >= index->count) {
        return PDF_ERROR(
            PDF_ERR_CFF_INVALID_OBJECT_IDX,
            "Cannot seek object %u in index of %u objects",
            (unsigned int)object_idx,
            (unsigned int)index->count
        );
    }

    size_t start_offset;
    size_t end_offset;
    PDF_PROPAGATE(
        cff_index_get_object_offset(index, parser, object_idx, &start_offset)
    );
    PDF_PROPAGATE(
        cff_index_get_object_offset(index, parser, object_idx + 1, &end_offset)
    );

    if (end_offset < start_offset) {
        return PDF_ERROR(
            PDF_ERR_CFF_INVALID_INDEX,
            "Objects in INDEX not in order"
        );
    }

    PDF_PROPAGATE(cff_parser_seek(parser, start_offset));

    *object_size_out = end_offset - start_offset;

    return NULL;
}

#ifdef TEST

TEST_FUNC(test_cff_index_empty_skip) {
    uint8_t buffer[] = {0x00, 0x00};
    CffParser parser = cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t));

    CffIndex index;
    TEST_PDF_REQUIRE(cff_parse_index(&parser, &index));

    TEST_PDF_REQUIRE(cff_index_skip(&index, &parser));
    TEST_ASSERT_EQ((size_t)2, parser.offset);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_index_empty_seek_object) {
    uint8_t buffer[] = {0x00, 0x00};
    CffParser parser = cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t));

    CffIndex index;
    TEST_PDF_REQUIRE(cff_parse_index(&parser, &index));

    size_t object_size;
    TEST_PDF_REQUIRE_ERR(
        cff_index_seek_object(&index, &parser, 0, &object_size),
        PDF_ERR_CFF_INVALID_OBJECT_IDX
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
    CffParser parser = cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t));

    CffIndex index;
    TEST_PDF_REQUIRE(cff_parse_index(&parser, &index));

    size_t object_size;
    TEST_PDF_REQUIRE(cff_index_seek_object(&index, &parser, 0, &object_size));
    TEST_ASSERT_EQ((size_t)7, parser.offset);
    TEST_ASSERT_EQ((size_t)3, object_size);

    TEST_PDF_REQUIRE(cff_index_seek_object(&index, &parser, 1, &object_size));
    TEST_ASSERT_EQ((size_t)10, parser.offset);
    TEST_ASSERT_EQ((size_t)2, object_size);

    TEST_PDF_REQUIRE(cff_index_seek_object(&index, &parser, 2, &object_size));
    TEST_ASSERT_EQ((size_t)12, parser.offset);
    TEST_ASSERT_EQ((size_t)1, object_size);

    TEST_PDF_REQUIRE_ERR(
        cff_index_seek_object(&index, &parser, 4, &object_size),
        PDF_ERR_CFF_INVALID_OBJECT_IDX
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
    CffParser parser = cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t));

    CffIndex index;
    TEST_PDF_REQUIRE(cff_parse_index(&parser, &index));

    size_t object_size;
    TEST_PDF_REQUIRE(cff_index_seek_object(&index, &parser, 0, &object_size));
    TEST_ASSERT_EQ((size_t)11, parser.offset);
    TEST_ASSERT_EQ((size_t)3, object_size);

    TEST_PDF_REQUIRE(cff_index_seek_object(&index, &parser, 1, &object_size));
    TEST_ASSERT_EQ((size_t)14, parser.offset);
    TEST_ASSERT_EQ((size_t)2, object_size);

    TEST_PDF_REQUIRE(cff_index_seek_object(&index, &parser, 2, &object_size));
    TEST_ASSERT_EQ((size_t)16, parser.offset);
    TEST_ASSERT_EQ((size_t)1, object_size);

    TEST_PDF_REQUIRE_ERR(
        cff_index_seek_object(&index, &parser, 4, &object_size),
        PDF_ERR_CFF_INVALID_OBJECT_IDX
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
    CffParser parser = cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t));

    CffIndex index;
    TEST_PDF_REQUIRE(cff_parse_index(&parser, &index));

    TEST_PDF_REQUIRE(cff_index_skip(&index, &parser));
    TEST_ASSERT_EQ((size_t)13, parser.offset);

    return TEST_RESULT_PASS;
}

#endif // TEST
