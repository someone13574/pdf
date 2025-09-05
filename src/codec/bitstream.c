#include "bitstream.h"

#include <stdint.h>
#include <stdio.h>

#include "logger/log.h"
#include "pdf_error/error.h"

BitStream bitstream_new(const uint8_t* data, size_t n_bytes) {
    return (BitStream) {.data = data, .length_bytes = n_bytes, .offset = 0};
}

PdfError* bitstream_next(BitStream* bitstream, uint32_t* out) {
    RELEASE_ASSERT(bitstream);
    RELEASE_ASSERT(out);

    size_t byte_offset = bitstream->offset >> 3;
    size_t bit_offset = bitstream->offset & 0x7;

    if (byte_offset >= bitstream->length_bytes) {
        return PDF_ERROR(
            PDF_ERR_CODEC_BITSTREAM_EOD,
            "Bitstream reached end-of-data"
        );
    }

    *out = (bitstream->data[byte_offset] >> bit_offset) & 0x1;
    bitstream->offset++;

    LOG_DIAG(
        TRACE,
        CODEC,
        "Read 1 bit from (offset %zu of) bitstream:  0x%x",
        bitstream->offset - 1,
        (unsigned int)*out
    );

    return NULL;
}

PdfError* bitstream_read_n(BitStream* bitstream, size_t n_bits, uint32_t* out) {
    RELEASE_ASSERT(bitstream);
    RELEASE_ASSERT(n_bits <= 32);
    RELEASE_ASSERT(out);
    *out = 0;

    size_t byte_offset = bitstream->offset >> 3;
    size_t bit_offset = bitstream->offset & 0x7;

    size_t write_offset = 0;
    while (n_bits != 0) {
        RELEASE_ASSERT(write_offset < 32);

        if (byte_offset >= bitstream->length_bytes) {
            return PDF_ERROR(
                PDF_ERR_CODEC_BITSTREAM_EOD,
                "Bitstream reached end-of-data during %zu bit read (start offset %zu in %zu bit stream)",
                n_bits + write_offset,
                bitstream->offset,
                bitstream->length_bytes * 8
            );
        }

        // Read aligned byte
        if (n_bits >= 8 && bit_offset == 0) {
            *out |= (uint32_t)bitstream->data[byte_offset]
                 << (uint32_t)write_offset;

            write_offset += 8;
            byte_offset += 1;
            n_bits -= 8;

            continue;
        }

        // Read unaligned
        size_t read_count = (n_bits > 8 - bit_offset) ? 8 - bit_offset : n_bits;
        uint8_t mask = (uint8_t)(UINT8_MAX >> (8 - read_count));

        uint8_t masked_data =
            (uint8_t)(bitstream->data[byte_offset] >> (bit_offset)) & mask;
        *out |= (uint32_t)masked_data << write_offset;

        write_offset += read_count;
        byte_offset += 1;
        bit_offset = 0;
        n_bits -= read_count;
    }

    bitstream->offset += write_offset;
    LOG_DIAG(
        TRACE,
        CODEC,
        "Read %zu bits from (offsets %zu-%zu of) bitstream: 0x%x (%u)",
        write_offset,
        bitstream->offset - write_offset,
        bitstream->offset,
        *out,
        (unsigned int)*out
    );

    return NULL;
}

void bitstream_align_byte(BitStream* bitstream) {
    RELEASE_ASSERT(bitstream);

    size_t byte_offset = bitstream->offset >> 3;
    size_t bit_offset = bitstream->offset & 0x7;

    if (bit_offset != 0) {
        bitstream->offset = (byte_offset + 1) << 3;
    }
}

size_t bitstream_remaining_bits(const BitStream* bitstream) {
    RELEASE_ASSERT(bitstream);

    return (bitstream->length_bytes << 3) - bitstream->offset;
}

#ifdef TEST

#include "test/test.h"

TEST_FUNC(test_bitstream_next) {
    uint8_t data[] = {0xd7, 0xa9};
    BitStream bitstream = bitstream_new(data, 2);

    uint32_t expected[] = {1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 0, 1, 0, 1, 0, 1};
    for (size_t idx = 0; idx < 16; idx++) {
        uint32_t value;
        TEST_PDF_REQUIRE(bitstream_next(&bitstream, &value));
        TEST_ASSERT_EQ(expected[idx], value, "Bit idx `%zu` didn't match", idx);
    }

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_bitstream_next_eod) {
    uint8_t data[] = {0xd7, 0xa9};
    BitStream bitstream = bitstream_new(data, 2);

    uint32_t expected[] = {1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 0, 1, 0, 1, 0, 1};
    for (size_t idx = 0; idx < 16; idx++) {
        uint32_t value;
        TEST_PDF_REQUIRE(bitstream_next(&bitstream, &value));
        TEST_ASSERT_EQ(expected[idx], value, "Bit idx `%zu` didn't match", idx);
    }

    uint32_t out;
    TEST_PDF_REQUIRE_ERR(
        bitstream_next(&bitstream, &out),
        PDF_ERR_CODEC_BITSTREAM_EOD
    );

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_bitstream_read_n_aligned) {
    uint8_t data[] = {0xd7, 0xa9};
    BitStream bitstream = bitstream_new(data, 2);

    uint32_t out;
    TEST_PDF_REQUIRE(bitstream_read_n(&bitstream, 8, &out));
    TEST_ASSERT_EQ((uint32_t)0xd7, out);

    TEST_PDF_REQUIRE(bitstream_read_n(&bitstream, 8, &out));
    TEST_ASSERT_EQ((uint32_t)0xa9, out);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_bitstream_read_n_unaligned) {
    uint8_t data[] = {0xd7, 0xa9};
    BitStream bitstream = bitstream_new(data, 2);

    uint32_t out;
    TEST_PDF_REQUIRE(bitstream_next(&bitstream, &out));
    TEST_ASSERT_EQ((uint32_t)1, out);

    TEST_PDF_REQUIRE(bitstream_read_n(&bitstream, 5, &out));
    TEST_ASSERT_EQ((uint32_t)0xb, out);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_bitstream_read_n_overlapping) {
    uint8_t data[] = {0xd7, 0xa9};
    BitStream bitstream = bitstream_new(data, 2);

    uint32_t out;
    TEST_PDF_REQUIRE(bitstream_next(&bitstream, &out));
    TEST_ASSERT_EQ((uint32_t)1, out);

    TEST_PDF_REQUIRE(bitstream_read_n(&bitstream, 12, &out));
    TEST_ASSERT_EQ((uint32_t)0x4eb, out);

    uint32_t expected[] = {1, 0, 1};
    for (size_t idx = 0; idx < 3; idx++) {
        uint32_t value;
        TEST_PDF_REQUIRE(bitstream_next(&bitstream, &value));
        TEST_ASSERT_EQ(expected[idx], value);
    }

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_bitstream_read_n_multi_bound) {
    uint8_t data[] = {0xd7, 0xa9, 0x42, 0x0d, 0xf0};
    BitStream bitstream = bitstream_new(data, 5);

    uint32_t out;
    TEST_PDF_REQUIRE(bitstream_next(&bitstream, &out));
    TEST_ASSERT_EQ((uint32_t)1, out);

    TEST_PDF_REQUIRE(bitstream_read_n(&bitstream, 32, &out));
    TEST_ASSERT_EQ((uint32_t)0x6a154eb, out);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_bitstream_read_n_eod) {
    uint8_t data[] = {0xd7, 0xa9, 0x42, 0x0d, 0xf0};
    BitStream bitstream = bitstream_new(data, 4);

    uint32_t out;
    TEST_PDF_REQUIRE(bitstream_next(&bitstream, &out));
    TEST_ASSERT_EQ((uint32_t)1, out);

    TEST_PDF_REQUIRE_ERR(
        bitstream_read_n(&bitstream, 32, &out),
        PDF_ERR_CODEC_BITSTREAM_EOD
    );

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_bitstream_align_byte) {
    uint8_t data[] = {0xd7, 0xa9, 0x42, 0x0d, 0xf0};
    BitStream bitstream = bitstream_new(data, 5);

    uint32_t out;
    TEST_PDF_REQUIRE(bitstream_read_n(&bitstream, 13, &out));

    bitstream_align_byte(&bitstream);
    TEST_ASSERT_EQ((size_t)16, bitstream.offset);

    bitstream_align_byte(&bitstream);
    TEST_ASSERT_EQ((size_t)16, bitstream.offset);

    TEST_PDF_REQUIRE(bitstream_next(&bitstream, &out));
    bitstream_align_byte(&bitstream);
    TEST_ASSERT_EQ((size_t)24, bitstream.offset);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_bitstream_remaining_bits) {
    uint8_t data[] = {0xd7, 0xa9, 0x42, 0x0d, 0xf0};
    BitStream bitstream = bitstream_new(data, 5);

    TEST_ASSERT_EQ((size_t)40, bitstream_remaining_bits(&bitstream));

    uint32_t out;
    TEST_PDF_REQUIRE(bitstream_read_n(&bitstream, 13, &out));
    TEST_ASSERT_EQ((size_t)27, bitstream_remaining_bits(&bitstream));

    return TEST_RESULT_PASS;
}

#endif // TEST
