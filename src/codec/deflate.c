#include "codec/deflate.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "bitstream.h"
#include "logger/log.h"
#include "pdf_error/error.h"

typedef struct {
    bool final;
    enum {
        DEFLATE_BLOCK_COMPRESSION_NONE,
        DEFLATE_BLOCK_COMPRESSION_FIXED,
        DEFLATE_BLOCK_COMPRESSION_DYN
    } type;
} DeflateBlockHeader;

static PdfError*
deflate_parse_block_header(BitStream* bitstream, DeflateBlockHeader* header) {
    RELEASE_ASSERT(bitstream);
    RELEASE_ASSERT(header);

    uint32_t read;
    PDF_PROPAGATE(bitstream_next(bitstream, &read));
    header->final = read == 1;

    PDF_PROPAGATE(bitstream_read_n(bitstream, 2, &read));
    switch (read) {
        case 0: {
            header->type = DEFLATE_BLOCK_COMPRESSION_NONE;
            break;
        }
        case 1: {
            header->type = DEFLATE_BLOCK_COMPRESSION_FIXED;
            break;
        }
        case 2: {
            header->type = DEFLATE_BLOCK_COMPRESSION_DYN;
            break;
        }
        default: {
            return PDF_ERROR(PDF_ERR_DEFLATE_INVALID_BLOCK_TYPE);
        }
    }

    return NULL;
}

static uint32_t reverse_bits(uint32_t x, int len) {
    uint32_t reversed = 0;
    while (len--) {
        reversed = (reversed << 1) | (x & 1);
        x >>= 1;
    }

    return reversed;
}

static void build_deflate_huffman_lut(
    uint8_t* bit_lens,
    size_t num_symbols,
    uint32_t* symbol_lut,
    uint32_t* length_lut,
    size_t max_bit_len
) {
    RELEASE_ASSERT(bit_lens);
    RELEASE_ASSERT(symbol_lut);
    RELEASE_ASSERT(length_lut);
    RELEASE_ASSERT(max_bit_len > 1);

    // Count the number of codes for each code length
    uint8_t len_counts[max_bit_len + 1];
    memset(len_counts, 0, sizeof(uint8_t) * (unsigned long)(max_bit_len + 1));

    for (size_t symbol_idx = 0; symbol_idx < num_symbols; symbol_idx++) {
        RELEASE_ASSERT(
            bit_lens[symbol_idx] <= max_bit_len && bit_lens[symbol_idx] > 0
        );
        len_counts[bit_lens[symbol_idx]] += 1;
    }

    size_t min_bit_len = max_bit_len;
    for (size_t bits = 1; bits <= max_bit_len; bits++) {
        if (len_counts[bits] != 0) {
            min_bit_len = bits;
            break;
        }
    }

    // Find the numerical value of the smallest code for each code length
    uint32_t curr_code = 0;
    uint32_t next_code[max_bit_len + 1];
    memset(next_code, 0, sizeof(uint32_t) * (unsigned long)(max_bit_len + 1));

    for (size_t bits = 1; bits <= max_bit_len; bits++) {
        curr_code = (curr_code + len_counts[bits - 1]) << 1;
        next_code[bits] = curr_code;
    }

    // Assign numerical values to all symbols
    for (size_t symbol_idx = 0; symbol_idx < num_symbols; symbol_idx++) {
        uint8_t bits = bit_lens[symbol_idx];
        uint32_t code = next_code[bits];
        uint32_t reversed_code = reverse_bits(code, (int)max_bit_len);

        symbol_lut[reversed_code] = (uint32_t)symbol_idx;
        length_lut[reversed_code] = bits;
        next_code[bits]++;
    }

    // Fill out empty entries
    for (uint32_t reversed_code = 0;
         reversed_code < (uint32_t)(1 << max_bit_len);
         reversed_code++) {
        if (length_lut[reversed_code] != 0) {
            continue;
        }

        // Find shortest valid code
        for (size_t bits = min_bit_len; bits < max_bit_len; bits++) {
            uint32_t mask = (uint32_t)(1 << (max_bit_len - bits)) - 1;
            uint32_t masked_reversed_code = reversed_code & ~mask;

            if (length_lut[masked_reversed_code] == (uint32_t)bits) {
                symbol_lut[reversed_code] = symbol_lut[masked_reversed_code];
                length_lut[reversed_code] = length_lut[masked_reversed_code];
                break;
            }
        }
    }
}

PdfError* decode_deflate_data(uint8_t* data, size_t data_len) {
    RELEASE_ASSERT(data);

    BitStream bitstream = bitstream_new(data, data_len);

    DeflateBlockHeader header;
    do {
        PDF_PROPAGATE(deflate_parse_block_header(&bitstream, &header));

        switch (header.type) {
            case DEFLATE_BLOCK_COMPRESSION_NONE: {
                bitstream_align_byte(&bitstream);

                uint32_t len;
                uint32_t n_len;
                PDF_PROPAGATE(bitstream_read_n(&bitstream, 16, &len));
                PDF_PROPAGATE(bitstream_read_n(&bitstream, 16, &n_len));

                if (len != ~len) {
                    return PDF_ERROR(
                        PDF_ERR_DEFLATE_LEN_COMPLIMENT,
                        "Uncompressed block length's one's compliment didn't match"
                    );
                }

                LOG_TODO("Copy %u uncompressed bytes from bitstream", len);
                break;
            }
            case DEFLATE_BLOCK_COMPRESSION_FIXED: {
                LOG_TODO();
                break;
            }
            case DEFLATE_BLOCK_COMPRESSION_DYN: {
                LOG_TODO();
                break;
            }
        }
    } while (!header.final);

    return NULL;
}

#ifdef TEST

#include "test/test.h"

TEST_FUNC(test_deflate_huffman) {
    uint8_t bit_lengths[] = {3, 3, 3, 3, 3, 2, 4, 4};
    uint32_t symbol_lut[16];
    uint32_t length_lut[16];

    build_deflate_huffman_lut(bit_lengths, 8, symbol_lut, length_lut, 4);

    // -1 in expected_symbols means there is no valid truncated form
    uint32_t expected_len[16] =
        {2, 2, 3, 2, 3, 3, 3, 4, 0, 0, 3, 3, 3, 3, 0, 4};
    int expected_symbols[16] =
        {5, 5, 2, 5, 0, 0, 4, 6, -1, -1, 3, 3, 1, 1, -1, 7};

    for (size_t idx = 0; idx < 16; idx++) {
        TEST_ASSERT_EQ(
            expected_len[idx],
            length_lut[idx],
            "Length mismatch for reverse code idx %zu",
            idx
        );

        TEST_ASSERT_EQ(
            expected_symbols[idx],
            length_lut[idx] == 0 ? -1 : (int)symbol_lut[idx],
            "Symbol mismatch for reverse code idx %zu",
            idx
        );
    }

    return TEST_RESULT_PASS;
}

#endif
