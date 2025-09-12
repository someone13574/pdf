#include "index.h"

#include <stdint.h>

#include "arena/arena.h"
#include "logger/log.h"
#include "parser.h"
#include "pdf_error/error.h"
#include "types.h"

#define DARRAY_NAME CffOffsetArray
#define DARRAY_LOWERCASE_NAME cff_offset_array
#define DARRAY_TYPE CffOffset
#include "arena/darray_impl.h"

#define DARRAY_NAME CffCard8Array
#define DARRAY_LOWERCASE_NAME cff_card8_array
#define DARRAY_TYPE CffCard8
#include "arena/darray_impl.h"

PdfError*
cff_parse_index(CffParser* parser, Arena* arena, CffIndex* index_out) {
    RELEASE_ASSERT(parser);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(index_out);

    PDF_PROPAGATE(cff_parser_read_card16(parser, &index_out->count));
    if (index_out->count == 0) {
        index_out->offset_size = 0;
        index_out->offset = NULL;
        index_out->data = NULL;
        return NULL;
    }

    PDF_PROPAGATE(cff_parser_read_offset_size(parser, &index_out->offset_size));

    index_out->offset = cff_offset_array_new(arena, index_out->count + 1);
    for (CffCard16 idx = 0; idx < index_out->count + 1; idx++) {
        CffOffset offset;
        PDF_PROPAGATE(
            cff_parser_read_offset(parser, index_out->offset_size, &offset)
        );
        cff_offset_array_set(index_out->offset, idx, offset);

        if (idx == 0 && offset != 1) {
            return PDF_ERROR(
                PDF_ERR_CFF_INVALID_INDEX,
                "First offset of index must be 1"
            );
        }
    }

    CffOffset last_offset;
    RELEASE_ASSERT(
        cff_offset_array_get(index_out->offset, index_out->count, &last_offset)
    );

    size_t data_len = last_offset - 1;
    index_out->data = cff_card8_array_new(arena, data_len);
    for (size_t idx = 0; idx < data_len; idx++) {
        CffCard8 card8;
        PDF_PROPAGATE(cff_parser_read_card8(parser, &card8));
        cff_card8_array_set(index_out->data, idx, card8);
    }

    return NULL;
}

#ifdef TEST

#include "test/test.h"

#define TEST_CHECK_EOF()                                                       \
    CffCard8 next_byte;                                                        \
    TEST_PDF_REQUIRE_ERR(                                                      \
        cff_parser_read_card8(&parser, &next_byte),                            \
        PDF_ERR_CFF_EOF                                                        \
    );

TEST_FUNC(test_cff_parse_index_empty) {
    uint8_t data[] = {0x00, 0x00};
    CffParser parser = cff_parser_new(data, sizeof(data) / sizeof(uint8_t));
    Arena* arena = arena_new(1024);

    CffIndex index;
    TEST_PDF_REQUIRE(cff_parse_index(&parser, arena, &index));
    TEST_ASSERT_EQ((CffCard16)0, index.count);

    TEST_CHECK_EOF()

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_parse_index_offsize1) {
    uint8_t buffer[] = {// count 1
                        0x00,
                        0x01,
                        // offset size 1
                        0x01,
                        // offsets 1, 4
                        0x01,
                        0x04,
                        // object data
                        'a',
                        'b',
                        'c'
    };
    CffParser parser = cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t));
    Arena* arena = arena_new(1024);

    CffIndex index;
    TEST_PDF_REQUIRE(cff_parse_index(&parser, arena, &index));
    TEST_ASSERT_EQ((CffCard16)1, index.count);
    TEST_ASSERT_EQ((CffOffsetSize)1, index.offset_size);

    CffOffset offset;
    TEST_ASSERT_EQ((size_t)2, cff_offset_array_len(index.offset));
    TEST_ASSERT(cff_offset_array_get(index.offset, 0, &offset));
    TEST_ASSERT_EQ((CffOffset)1, offset);
    TEST_ASSERT(cff_offset_array_get(index.offset, 1, &offset));
    TEST_ASSERT_EQ((CffOffset)4, offset);

    CffCard8 data;
    TEST_ASSERT_EQ((size_t)3, cff_card8_array_len(index.data));
    TEST_ASSERT(cff_card8_array_get(index.data, 0, &data));
    TEST_ASSERT_EQ((CffCard8)'a', data);
    TEST_ASSERT(cff_card8_array_get(index.data, 1, &data));
    TEST_ASSERT_EQ((CffCard8)'b', data);
    TEST_ASSERT(cff_card8_array_get(index.data, 2, &data));
    TEST_ASSERT_EQ((CffCard8)'c', data);

    TEST_CHECK_EOF()

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_parse_index_offsize3) {
    uint8_t buffer[] = {// count 1
                        0x00,
                        0x01,
                        // offset size 3
                        0x03,
                        // offsets 1, 4
                        0x00,
                        0x00,
                        0x01,
                        0x00,
                        0x00,
                        0x04,
                        // object data
                        'a',
                        'b',
                        'c'
    };
    CffParser parser = cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t));
    Arena* arena = arena_new(1024);

    CffIndex index;
    TEST_PDF_REQUIRE(cff_parse_index(&parser, arena, &index));
    TEST_ASSERT_EQ((CffCard16)1, index.count);
    TEST_ASSERT_EQ((CffOffsetSize)3, index.offset_size);

    CffOffset offset;
    TEST_ASSERT_EQ((size_t)2, cff_offset_array_len(index.offset));
    TEST_ASSERT(cff_offset_array_get(index.offset, 0, &offset));
    TEST_ASSERT_EQ((CffOffset)1, offset);
    TEST_ASSERT(cff_offset_array_get(index.offset, 1, &offset));
    TEST_ASSERT_EQ((CffOffset)4, offset);

    CffCard8 data;
    TEST_ASSERT_EQ((size_t)3, cff_card8_array_len(index.data));
    TEST_ASSERT(cff_card8_array_get(index.data, 0, &data));
    TEST_ASSERT_EQ((CffCard8)'a', data);
    TEST_ASSERT(cff_card8_array_get(index.data, 1, &data));
    TEST_ASSERT_EQ((CffCard8)'b', data);
    TEST_ASSERT(cff_card8_array_get(index.data, 2, &data));
    TEST_ASSERT_EQ((CffCard8)'c', data);

    TEST_CHECK_EOF()

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_cff_parse_index_multi_object) {
    uint8_t buffer[] = {// count 2
                        0x00,
                        0x02,
                        // offset size 1
                        0x01,
                        // offsets 1, 4, 6
                        0x01,
                        0x04,
                        0x06,
                        // object data
                        'a',
                        'b',
                        'c',
                        4,
                        2
    };
    CffParser parser = cff_parser_new(buffer, sizeof(buffer) / sizeof(uint8_t));
    Arena* arena = arena_new(1024);

    CffIndex index;
    TEST_PDF_REQUIRE(cff_parse_index(&parser, arena, &index));
    TEST_ASSERT_EQ((CffCard16)2, index.count);
    TEST_ASSERT_EQ((CffOffsetSize)1, index.offset_size);

    CffOffset offset;
    TEST_ASSERT_EQ((size_t)3, cff_offset_array_len(index.offset));
    TEST_ASSERT(cff_offset_array_get(index.offset, 0, &offset));
    TEST_ASSERT_EQ((CffOffset)1, offset);
    TEST_ASSERT(cff_offset_array_get(index.offset, 1, &offset));
    TEST_ASSERT_EQ((CffOffset)4, offset);
    TEST_ASSERT(cff_offset_array_get(index.offset, 2, &offset));
    TEST_ASSERT_EQ((CffOffset)6, offset);

    CffCard8 data;
    TEST_ASSERT_EQ((size_t)5, cff_card8_array_len(index.data));
    TEST_ASSERT(cff_card8_array_get(index.data, 0, &data));
    TEST_ASSERT_EQ((CffCard8)'a', data);
    TEST_ASSERT(cff_card8_array_get(index.data, 1, &data));
    TEST_ASSERT_EQ((CffCard8)'b', data);
    TEST_ASSERT(cff_card8_array_get(index.data, 2, &data));
    TEST_ASSERT_EQ((CffCard8)'c', data);
    TEST_ASSERT(cff_card8_array_get(index.data, 3, &data));
    TEST_ASSERT_EQ((CffCard8)4, data);
    TEST_ASSERT(cff_card8_array_get(index.data, 4, &data));
    TEST_ASSERT_EQ((CffCard8)2, data);

    TEST_CHECK_EOF()

    arena_free(arena);
    return TEST_RESULT_PASS;
}

#undef TEST_CHECK_EOF
#endif // TEST
