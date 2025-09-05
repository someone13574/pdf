#include "deflate.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "arena/arena.h"
#include "arena/common.h"
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
            LOG_DIAG(
                DEBUG,
                CODEC,
                "Deflate block type: DEFLATE_BLOCK_COMPRESSION_NONE"
            );
            break;
        }
        case 1: {
            header->type = DEFLATE_BLOCK_COMPRESSION_FIXED;
            LOG_DIAG(
                DEBUG,
                CODEC,
                "Deflate block type: DEFLATE_BLOCK_COMPRESSION_FIXED"
            );
            break;
        }
        case 2: {
            header->type = DEFLATE_BLOCK_COMPRESSION_DYN;
            LOG_DIAG(
                DEBUG,
                CODEC,
                "Deflate block type: DEFLATE_BLOCK_COMPRESSION_DYN"
            );
            break;
        }
        default: {
            return PDF_ERROR(PDF_ERR_DEFLATE_INVALID_BLOCK_TYPE);
        }
    }

    return NULL;
}

typedef struct {
    size_t
        lut_length; /// Number of entries in the symbol and length lookup tables
    size_t min_bit_len; /// Minimum bit length of a code
    size_t max_bit_len; /// Maximum bit length of a code

    uint32_t* symbol_lut; /// Mapping from reversed (lsb) codes to symbol
                          /// indices. TODO: test smaller datatype
    uint8_t*
        length_lut; /// Mapping from reversed (lsb) codes to code bit lengths
} DeflateHuffmanLut;

static uint32_t reverse_bits(uint32_t x, int len) {
    uint32_t reversed = 0;
    while (len--) {
        reversed = (reversed << 1) | (x & 1);
        x >>= 1;
    }

    return reversed;
}

static DeflateHuffmanLut
build_deflate_huffman_lut(Arena* arena, uint8_t* bit_lens, size_t num_symbols) {
    RELEASE_ASSERT(bit_lens);

    LOG_DIAG(
        DEBUG,
        CODEC,
        "Building deflate huffman lut with %zu symbols",
        num_symbols
    );

    // Count the number of codes for each code length
    size_t len_counts[16];
    memset(len_counts, 0, sizeof(size_t) * 16);

    for (size_t symbol_idx = 0; symbol_idx < num_symbols; symbol_idx++) {
        RELEASE_ASSERT(bit_lens[symbol_idx] < 16);
        len_counts[bit_lens[symbol_idx]] += 1;
    }

    len_counts[0] = 0; // ignore zero-length codes

    size_t min_bit_len = SIZE_MAX;
    size_t max_bit_len = 0;
    for (size_t bits = 1; bits < 16; bits++) {
        if (len_counts[bits] == 0) {
            continue;
        }

        if (bits < min_bit_len) {
            min_bit_len = bits;
        }

        if (bits > max_bit_len) {
            max_bit_len = bits;
        }
    }

    DeflateHuffmanLut lut;
    lut.lut_length = 1 << max_bit_len;
    lut.min_bit_len = min_bit_len;
    lut.max_bit_len = max_bit_len;
    lut.symbol_lut = arena_alloc(arena, sizeof(uint32_t) * lut.lut_length);
    lut.length_lut = arena_alloc(arena, sizeof(uint8_t) * lut.lut_length);

    memset(lut.symbol_lut, 0, sizeof(uint32_t) * lut.lut_length);
    memset(lut.length_lut, 0, sizeof(uint8_t) * lut.lut_length);

    // Find the numerical value of the smallest code for each code length
    uint32_t curr_code = 0;
    uint32_t next_code[max_bit_len + 1];
    memset(next_code, 0, sizeof(uint32_t) * (unsigned long)(max_bit_len + 1));

    for (size_t bits = 1; bits <= max_bit_len; bits++) {
        curr_code = (curr_code + (uint32_t)len_counts[bits - 1]) << 1;
        next_code[bits] = curr_code;
    }

    // Assign numerical values to all symbols
    for (size_t symbol_idx = 0; symbol_idx < num_symbols; symbol_idx++) {
        uint8_t bits = bit_lens[symbol_idx];
        if (bits == 0) {
            continue;
        }

        uint32_t code = next_code[bits];
        uint32_t reversed_code = reverse_bits(code, (int)bits);

        RELEASE_ASSERT(reversed_code < lut.lut_length);
        RELEASE_ASSERT(lut.length_lut[reversed_code] == 0);

        lut.symbol_lut[reversed_code] = (uint32_t)symbol_idx;
        lut.length_lut[reversed_code] = bits;
        next_code[bits]++;
    }

    // Fill out empty entries with prefix codes
    for (uint32_t code = 0; code < lut.lut_length; code++) {
        if (lut.length_lut[code] != 0) {
            continue;
        }

        // Find longest valid code
        for (int bits = (int)max_bit_len; bits >= (int)min_bit_len; bits--) {
            uint32_t mask = (uint32_t)(1 << bits) - 1;
            uint32_t masked_code = code & mask;

            RELEASE_ASSERT(masked_code < lut.lut_length);

            if (lut.length_lut[masked_code] == (uint32_t)bits) {
                lut.symbol_lut[code] = lut.symbol_lut[masked_code];
                lut.length_lut[code] = lut.length_lut[masked_code];
                break;
            }
        }
    }

    for (size_t idx = 0; idx < lut.lut_length; idx++) {
        LOG_DIAG(
            TRACE,
            CODEC,
            "       key=0x%03x  symbol=%-3u  bits=%u",
            (unsigned int)idx,
            (unsigned int)lut.symbol_lut[idx],
            (unsigned int)lut.length_lut[idx]
        );
    }

    return lut;
}

static PdfError* deflate_huffman_decode(
    BitStream* bitstream,
    DeflateHuffmanLut* lut,
    uint32_t* symbol_out
) {
    RELEASE_ASSERT(bitstream);
    RELEASE_ASSERT(lut);
    RELEASE_ASSERT(symbol_out);

    // Fast path when max bit len is available
    if (bitstream_remaining_bits(bitstream) >= lut->max_bit_len) {
        LOG_DIAG(TRACE, CODEC, "Fast-path decoding stream");

        uint32_t encoded_symbol;
        PDF_PROPAGATE(
            bitstream_read_n(bitstream, lut->max_bit_len, &encoded_symbol)
        );

        LOG_DIAG(
            TRACE,
            CODEC,
            "Decoded huffman symbol=%u, length=%u\n",
            (unsigned int)lut->symbol_lut[encoded_symbol],
            (unsigned int)lut->length_lut[encoded_symbol]
        );

        uint8_t symbol_len = lut->length_lut[encoded_symbol];
        bitstream->offset -= lut->max_bit_len - symbol_len;
        if (symbol_len == 0) {
            return PDF_ERROR(PDF_ERR_DEFLATE_INVALID_SYMBOL);
        }

        *symbol_out = lut->symbol_lut[encoded_symbol];
        return NULL;
    }

    // Iteratively read bit lengths until a match is found (prevents reads past
    // bitstream eod)
    size_t bits = lut->min_bit_len;

    LOG_DIAG(TRACE, CODEC, "Iteratively decoding bitstream");

    uint32_t buffer;
    PDF_PROPAGATE(bitstream_read_n(bitstream, bits, &buffer));

    do {
        uint32_t symbol = lut->symbol_lut[buffer];
        uint32_t symbol_len = lut->length_lut[buffer];

        LOG_DIAG(
            TRACE,
            CODEC,
            "Attempted decoded huffman key=0x%x (len=%u), out_symbol=%u, length=%u",
            (unsigned int)buffer,
            (unsigned int)bits,
            (unsigned int)symbol,
            (unsigned int)symbol_len
        );

        if (symbol_len == bits) {
            LOG_DIAG(
                TRACE,
                CODEC,
                "Decode successful symbol=%u, length=%u\n",
                (unsigned int)symbol,
                (unsigned int)symbol_len
            );

            *symbol_out = symbol;
            return NULL;
        }

        // Add another bit
        uint32_t next_bit;
        PDF_PROPAGATE(bitstream_next(bitstream, &next_bit));

        buffer |= next_bit << bits;
        bits++;
    } while (bits <= lut->max_bit_len);

    return PDF_ERROR(PDF_ERR_DEFLATE_INVALID_SYMBOL);
}

static PdfError* decode_dyn_huffman_table_luts(
    Arena* arena,
    BitStream* bitstream,
    DeflateHuffmanLut* lit_lut_ptr,
    DeflateHuffmanLut* dist_lut_ptr
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(bitstream);
    RELEASE_ASSERT(lit_lut_ptr);
    RELEASE_ASSERT(dist_lut_ptr);

    LOG_DIAG(DEBUG, CODEC, "Reading dyn block header...");

    uint32_t num_lit_code_lens;
    PDF_PROPAGATE(bitstream_read_n(bitstream, 5, &num_lit_code_lens));
    if (num_lit_code_lens >= 30) {
        LOG_WARN(
            CODEC,
            "HLIT should be in the range 257 - 286, though higher values are representable"
        );
    }
    num_lit_code_lens += 257;

    uint32_t num_dist_code_lens;
    PDF_PROPAGATE(bitstream_read_n(bitstream, 5, &num_dist_code_lens));
    num_dist_code_lens += 1;

    uint32_t num_code_len_symbols;
    PDF_PROPAGATE(bitstream_read_n(bitstream, 4, &num_code_len_symbols));
    num_code_len_symbols += 4;

    LOG_DIAG(
        DEBUG,
        CODEC,
        "Dyn block header: HLIT: %u + 257 = %u     HDIST: %u + 1 = %u     HCLEN: %u + 4 = %u",
        (unsigned int)(num_lit_code_lens - 257),
        (unsigned int)num_lit_code_lens,
        (unsigned int)(num_dist_code_lens - 1),
        (unsigned int)num_dist_code_lens,
        (unsigned int)(num_code_len_symbols - 4),
        (unsigned int)num_code_len_symbols
    );

    LOG_DIAG(DEBUG, CODEC, "Reading code length bit lengths");

    // Build code length lut
    uint8_t code_length_bit_lens[19];
    memset(code_length_bit_lens, 0, 19);

    uint8_t code_length_swizzle[] =
        {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

    for (size_t idx = 0; idx < num_code_len_symbols; idx++) {
        uint32_t symbol_bit_len;
        PDF_PROPAGATE(bitstream_read_n(bitstream, 3, &symbol_bit_len));
        code_length_bit_lens[code_length_swizzle[idx]] =
            (uint8_t)symbol_bit_len;
    }

    DeflateHuffmanLut code_len_lut =
        build_deflate_huffman_lut(arena, code_length_bit_lens, 19);

    // Read bit lengths (must be done in single pass since repeats can cross
    // from the lit table to the dist table)
    uint8_t bit_lens[num_lit_code_lens + num_dist_code_lens];
    size_t bit_len_offset = 0;

    LOG_DIAG(
        DEBUG,
        CODEC,
        "Getting %zu bit lens",
        (size_t)(num_lit_code_lens + num_dist_code_lens)
    );

    while (bit_len_offset < num_lit_code_lens + num_dist_code_lens) {
        uint32_t symbol;
        PDF_PROPAGATE(deflate_huffman_decode(bitstream, &code_len_lut, &symbol)
        );

        if (symbol <= 15) {
            bit_lens[bit_len_offset++] = (uint8_t)symbol;
            LOG_DIAG(
                TRACE,
                CODEC,
                "Got literal bit length %u",
                (unsigned int)symbol
            );
        } else {
            uint8_t repeat_val = 0;
            uint8_t repeat_count_offset = 0;
            uint32_t repeat_count_bits = 0;

            if (symbol == 16) {
                if (bit_len_offset == 0) {
                    return PDF_ERROR(PDF_ERR_DEFLATE_REPEAT_UNDERFLOW);
                }

                repeat_val = bit_lens[bit_len_offset - 1];
                repeat_count_offset = 3;
                repeat_count_bits = 2;
            } else if (symbol == 17) {
                repeat_val = 0;
                repeat_count_offset = 3;
                repeat_count_bits = 3;
            } else if (symbol == 18) {
                repeat_val = 0;
                repeat_count_offset = 11;
                repeat_count_bits = 7;
            }

            uint32_t repeat_count;
            PDF_PROPAGATE(
                bitstream_read_n(bitstream, repeat_count_bits, &repeat_count)
            );
            repeat_count += repeat_count_offset;
            LOG_DIAG(
                TRACE,
                CODEC,
                "Repeating bit_length=%u, %u times",
                (unsigned int)repeat_val,
                (unsigned int)repeat_count
            );

            if (bit_len_offset + repeat_count
                > num_lit_code_lens + num_dist_code_lens) {
                return PDF_ERROR(PDF_ERR_DEFLATE_REPEAT_OVERFLOW);
            }

            for (size_t idx = 0; idx < repeat_count; idx++) {
                bit_lens[bit_len_offset++] = repeat_val;
            }
        }
    }

    // Create luts
    DeflateHuffmanLut lit_lut =
        build_deflate_huffman_lut(arena, bit_lens, num_lit_code_lens);
    DeflateHuffmanLut dist_lut = build_deflate_huffman_lut(
        arena,
        bit_lens + num_lit_code_lens,
        num_dist_code_lens
    );

    *lit_lut_ptr = lit_lut;
    *dist_lut_ptr = dist_lut;

    return NULL;
}

static DeflateHuffmanLut get_fixed_huffman_lut(void) {
    static bool initialized = false;
    static DeflateHuffmanLut lut;

    if (!initialized) {
        LOG_DIAG(DEBUG, CODEC, "Initializing fixed deflate huffman lut");

        initialized = true;

        uint8_t bit_lens[288];

        for (size_t i = 0; i < 144; i++) {
            bit_lens[i] = 8;
        }
        for (size_t i = 144; i < 256; i++) {
            bit_lens[i] = 9;
        }
        for (size_t i = 256; i < 280; i++) {
            bit_lens[i] = 7;
        }
        for (size_t i = 280; i < 288; i++) {
            bit_lens[i] = 8;
        }

        Arena* persistent_arena = arena_new(1);
        lut = build_deflate_huffman_lut(persistent_arena, bit_lens, 288);
    }

    return lut;
}

static PdfError* deflate_decode_length_code(
    BitStream* bitstream,
    uint32_t lit_symbol,
    uint32_t* length
) {
    RELEASE_ASSERT(bitstream);
    RELEASE_ASSERT(lit_symbol >= 257 && lit_symbol <= 285);
    RELEASE_ASSERT(length);

    LOG_DIAG(DEBUG, CODEC, "Reading length code...");

    uint32_t length_offsets[] = {3,  4,  5,  6,   7,   8,   9,   10,  11, 13,
                                 15, 17, 19, 23,  27,  31,  35,  43,  51, 59,
                                 67, 83, 99, 115, 131, 163, 195, 227, 258};
    uint32_t length_bits[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                              2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};

    *length = length_offsets[lit_symbol - 257];
    if (length_bits[lit_symbol - 257]) {
        uint32_t extra;
        PDF_PROPAGATE(
            bitstream_read_n(bitstream, length_bits[lit_symbol - 257], &extra)
        );
        *length += extra;

        if (*length == 258 && lit_symbol == 284) {
            LOG_WARN(
                CODEC,
                "RFC 1951 specifies that length code 284 encodes from 227-257, though 258 is representable"
            );
        }
    }

    LOG_DIAG(TRACE, CODEC, "Backref length: %u", (unsigned int)*length);

    return NULL;
}

static PdfError* deflate_decode_distance_code(
    BitStream* bitstream,
    DeflateHuffmanLut* dist_lut,
    uint32_t* distance
) {
    RELEASE_ASSERT(bitstream);
    RELEASE_ASSERT(distance);

    LOG_DIAG(DEBUG, CODEC, "Reading distance code...");

    // Get distance symbol
    uint32_t dist_symbol = 0;
    if (dist_lut) {
        PDF_PROPAGATE(deflate_huffman_decode(bitstream, dist_lut, &dist_symbol)
        );
    } else {
        PDF_PROPAGATE(bitstream_read_n(bitstream, 5, &dist_symbol));
        dist_symbol = reverse_bits(dist_symbol, 5);
    }

    // Lookup details
    uint32_t dist_offsets[] = {1,    2,    3,    4,     5,     7,    9,    13,
                               17,   25,   33,   49,    65,    97,   129,  193,
                               257,  385,  513,  769,   1025,  1537, 2049, 3073,
                               4097, 6145, 8193, 12289, 16385, 24577};
    uint32_t dist_bits[] = {0, 0, 0,  0,  1,  1,  2,  2,  3,  3,
                            4, 4, 5,  5,  6,  6,  7,  7,  8,  8,
                            9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

    if (dist_symbol >= 30) {
        return PDF_ERROR(
            PDF_ERR_DEFLATE_INVALID_SYMBOL,
            "Invalid distance symbol %u",
            (unsigned int)dist_symbol
        );
    }

    // Get distance
    *distance = dist_offsets[dist_symbol];
    if (dist_bits[dist_symbol] != 0) {
        uint32_t extra;
        PDF_PROPAGATE(
            bitstream_read_n(bitstream, dist_bits[dist_symbol], &extra)
        );
        *distance += extra;
    }

    LOG_DIAG(TRACE, CODEC, "Backref distance: %u", (unsigned int)*distance);

    return NULL;
}

PdfError* decode_deflate_data(
    Arena* arena,
    BitStream* bitstream,
    Uint8Array** decoded_bytes
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(bitstream);
    RELEASE_ASSERT(decoded_bytes);

    LOG_DIAG(INFO, CODEC, "Decoding deflate stream");

    Arena* block_arena = arena_new(1024);
    Arena* decode_arena = arena_new(1024);

    Uint8Vec* output_stream = uint8_vec_new(decode_arena);

    DeflateBlockHeader header;
    do {
        PDF_PROPAGATE(deflate_parse_block_header(bitstream, &header));

        if (header.type == DEFLATE_BLOCK_COMPRESSION_NONE) {
            bitstream_align_byte(bitstream);

            uint32_t len;
            uint32_t n_len;
            PDF_PROPAGATE(bitstream_read_n(bitstream, 16, &len));
            PDF_PROPAGATE(bitstream_read_n(bitstream, 16, &n_len));

            if ((uint16_t)len != (uint16_t)~n_len) {
                return PDF_ERROR(
                    PDF_ERR_DEFLATE_LEN_COMPLIMENT,
                    "Uncompressed block length's one's compliment didn't match"
                );
            }

            LOG_DIAG(
                DEBUG,
                CODEC,
                "Reading %u uncompressed bytes",
                (unsigned int)len
            );

            for (size_t idx = 0; idx < len; idx++) {
                uint32_t byte;
                PDF_PROPAGATE(bitstream_read_n(bitstream, 8, &byte));

                uint8_vec_push(output_stream, (uint8_t)byte);
            }
        } else {
            DeflateHuffmanLut lit_lut;
            DeflateHuffmanLut dist_lut; // only used for dynamic blocks

            if (header.type == DEFLATE_BLOCK_COMPRESSION_DYN) {
                PDF_PROPAGATE(decode_dyn_huffman_table_luts(
                    block_arena,
                    bitstream,
                    &lit_lut,
                    &dist_lut
                ));
            } else {
                lit_lut = get_fixed_huffman_lut();
            }

            do {
                uint32_t lit_symbol;
                PDF_PROPAGATE(
                    deflate_huffman_decode(bitstream, &lit_lut, &lit_symbol)
                );

                if (lit_symbol < 256) {
                    uint8_vec_push(output_stream, (uint8_t)lit_symbol);
                } else if (lit_symbol == 256) {
                    break;
                } else {
                    uint32_t length;
                    PDF_PROPAGATE(deflate_decode_length_code(
                        bitstream,
                        lit_symbol,
                        &length
                    ));

                    uint32_t distance;
                    PDF_PROPAGATE(deflate_decode_distance_code(
                        bitstream,
                        header.type == DEFLATE_BLOCK_COMPRESSION_FIXED
                            ? NULL
                            : &dist_lut,
                        &distance
                    ));

                    uint32_t output_stream_len =
                        (uint32_t)uint8_vec_len(output_stream);
                    if (output_stream_len < distance) {
                        return PDF_ERROR(
                            PDF_ERR_DEFLATE_BACKREF_UNDERFLOW,
                            "Attempted to backreference to %d",
                            (int)output_stream_len - (int)distance
                        );
                    }

                    for (size_t idx = output_stream_len - distance;
                         idx < output_stream_len - distance + length;
                         idx++) {
                        uint8_t byte;
                        RELEASE_ASSERT(uint8_vec_get(output_stream, idx, &byte)
                        );

                        uint8_vec_push(output_stream, byte);
                    }
                }

                LOG_DIAG(
                    TRACE,
                    CODEC,
                    "Output stream is now %zu byes",
                    uint8_vec_len(output_stream)
                );
            } while (true);
        }

        arena_reset(block_arena); // invalidates dyn block luts
    } while (!header.final);

    // Copy output bytes to array
    *decoded_bytes = uint8_array_new(arena, uint8_vec_len(output_stream));
    for (size_t idx = 0; idx < uint8_vec_len(output_stream); idx++) {
        uint8_t byte;
        RELEASE_ASSERT(uint8_vec_get(output_stream, idx, &byte));
        uint8_array_set(*decoded_bytes, idx, byte);
    }

    arena_free(block_arena);
    arena_free(decode_arena);
    return NULL;
}

#ifdef TEST

#include "test/test.h"

// Test cases taken from:
// https://github.com/nayuki/Simple-DEFLATE-decompressor/blob/master/java/test/DecompressorTest.java

TEST_FUNC(test_deflate_huffman) {
    uint8_t bit_lengths[] = {3, 3, 3, 3, 3, 2, 4, 4};

    Arena* lut_arena = arena_new(1024);
    DeflateHuffmanLut lut =
        build_deflate_huffman_lut(lut_arena, bit_lengths, 8);

    TEST_ASSERT_EQ((size_t)16, lut.lut_length);
    TEST_ASSERT_EQ((size_t)2, lut.min_bit_len);
    TEST_ASSERT_EQ((size_t)4, lut.max_bit_len);

    uint8_t test_data[] = {0x72, 0x3a, 0xee, 0x8f, 0x35, 0x16};
    BitStream bitstream = bitstream_new(test_data, 6);

    uint32_t expected_symbols[] = {0, 1, 2, 3, 4, 5, 6, 7, 6, 5, 4, 3, 2, 1, 0};
    for (size_t idx = 0; idx < 15; idx++) {
        uint32_t symbol;
        TEST_PDF_REQUIRE(deflate_huffman_decode(&bitstream, &lut, &symbol));
        TEST_ASSERT_EQ(
            expected_symbols[idx],
            symbol,
            "Symbol at idx %zu didn't match",
            idx
        );
    }

    arena_free(lut_arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_deflate_uncompressed_empty) {
    uint8_t stream[] = {0x01, 0x00, 0x00, 0xff, 0xff};
    BitStream bitstream =
        bitstream_new(stream, sizeof(stream) / sizeof(uint8_t));

    Arena* arena = arena_new(1024);
    Uint8Array* out = NULL;
    TEST_PDF_REQUIRE(decode_deflate_data(arena, &bitstream, &out));

    TEST_ASSERT_EQ((size_t)0, uint8_array_len(out));

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_deflate_uncompressed_multibyte) {
    uint8_t stream[] = {0x01, 0x03, 0x00, 0xfc, 0xff, 0x05, 0x14, 0x23};
    BitStream bitstream =
        bitstream_new(stream, sizeof(stream) / sizeof(uint8_t));

    Arena* arena = arena_new(1024);
    Uint8Array* out = NULL;
    TEST_PDF_REQUIRE(decode_deflate_data(arena, &bitstream, &out));

    uint8_t expected[] = {0x05, 0x14, 0x23};
    TEST_ASSERT_EQ(
        (size_t)(sizeof(expected) / sizeof(uint8_t)),
        uint8_array_len(out)
    );

    for (size_t idx = 0; idx < sizeof(expected) / sizeof(uint8_t); idx++) {
        uint8_t value;
        TEST_ASSERT(uint8_array_get(out, idx, &value));
        TEST_ASSERT_EQ(
            expected[idx],
            value,
            "Decoded value at idx %zu was incorrect",
            idx
        );
    }

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_deflate_uncompressed_multiblock) {
    uint8_t stream[] = {
        0x00,
        0x02,
        0x00,
        0xfd,
        0xff,
        0x05,
        0x14,
        0x01,
        0x01,
        0x00,
        0xfe,
        0xff,
        0x23
    };
    BitStream bitstream =
        bitstream_new(stream, sizeof(stream) / sizeof(uint8_t));

    Arena* arena = arena_new(1024);
    Uint8Array* out = NULL;
    TEST_PDF_REQUIRE(decode_deflate_data(arena, &bitstream, &out));

    uint8_t expected[] = {0x05, 0x14, 0x23};
    TEST_ASSERT_EQ(
        (size_t)(sizeof(expected) / sizeof(uint8_t)),
        uint8_array_len(out)
    );

    for (size_t idx = 0; idx < sizeof(expected) / sizeof(uint8_t); idx++) {
        uint8_t value;
        TEST_ASSERT(uint8_array_get(out, idx, &value));
        TEST_ASSERT_EQ(
            expected[idx],
            value,
            "Decoded value at idx %zu was incorrect",
            idx
        );
    }

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_deflate_uncompressed_no_discard) {
    uint8_t stream[] =
        {0x9a, 0xb0, 0xf0, 0x3f, 0x20, 0x02, 0x00, 0xfd, 0xff, 0xab, 0xcd};
    BitStream bitstream =
        bitstream_new(stream, sizeof(stream) / sizeof(uint8_t));

    Arena* arena = arena_new(1024);
    Uint8Array* out = NULL;
    TEST_PDF_REQUIRE(decode_deflate_data(arena, &bitstream, &out));

    uint8_t expected[] = {0x90, 0xa1, 0xff, 0xab, 0xcd};
    TEST_ASSERT_EQ(
        (size_t)(sizeof(expected) / sizeof(uint8_t)),
        uint8_array_len(out)
    );

    for (size_t idx = 0; idx < sizeof(expected) / sizeof(uint8_t); idx++) {
        uint8_t value;
        TEST_ASSERT(uint8_array_get(out, idx, &value));
        TEST_ASSERT_EQ(
            expected[idx],
            value,
            "Decoded value at idx %zu was incorrect",
            idx
        );
    }

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_deflate_fixed_empty) {
    uint8_t stream[] = {0x03, 0x00};
    BitStream bitstream =
        bitstream_new(stream, sizeof(stream) / sizeof(uint8_t));

    Arena* arena = arena_new(1024);
    Uint8Array* out = NULL;
    TEST_PDF_REQUIRE(decode_deflate_data(arena, &bitstream, &out));

    TEST_ASSERT_EQ((size_t)0, uint8_array_len(out));

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_deflate_fixed_literals) {
    uint8_t stream[] = {0x63, 0x68, 0xe8, 0x9f, 0x70, 0xe0, 0x3f, 0x00};
    BitStream bitstream =
        bitstream_new(stream, sizeof(stream) / sizeof(uint8_t));

    Arena* arena = arena_new(1024);
    Uint8Array* out = NULL;
    TEST_PDF_REQUIRE(decode_deflate_data(arena, &bitstream, &out));

    uint8_t expected[] = {0x00, 0x80, 0x8f, 0x90, 0xc0, 0xff};
    TEST_ASSERT_EQ(
        (size_t)(sizeof(expected) / sizeof(uint8_t)),
        uint8_array_len(out)
    );

    for (size_t idx = 0; idx < sizeof(expected) / sizeof(uint8_t); idx++) {
        uint8_t value;
        TEST_ASSERT(uint8_array_get(out, idx, &value));
        TEST_ASSERT_EQ(
            expected[idx],
            value,
            "Decoded value at idx %zu was incorrect",
            idx
        );
    }

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_deflate_fixed_non_overlapping) {
    uint8_t stream[] = {0x63, 0x60, 0x64, 0x02, 0x22, 0x00};
    BitStream bitstream =
        bitstream_new(stream, sizeof(stream) / sizeof(uint8_t));

    Arena* arena = arena_new(1024);
    Uint8Array* out = NULL;
    TEST_PDF_REQUIRE(decode_deflate_data(arena, &bitstream, &out));

    uint8_t expected[] = {0x00, 0x01, 0x02, 0x00, 0x01, 0x02};
    TEST_ASSERT_EQ(
        (size_t)(sizeof(expected) / sizeof(uint8_t)),
        uint8_array_len(out)
    );

    for (size_t idx = 0; idx < sizeof(expected) / sizeof(uint8_t); idx++) {
        uint8_t value;
        TEST_ASSERT(uint8_array_get(out, idx, &value));
        TEST_ASSERT_EQ(
            expected[idx],
            value,
            "Decoded value at idx %zu was incorrect",
            idx
        );
    }

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_deflate_fixed_overlapping) {
    uint8_t stream[] = {0x63, 0x04, 0x01, 0x00};
    BitStream bitstream =
        bitstream_new(stream, sizeof(stream) / sizeof(uint8_t));

    Arena* arena = arena_new(1024);
    Uint8Array* out = NULL;
    TEST_PDF_REQUIRE(decode_deflate_data(arena, &bitstream, &out));

    uint8_t expected[] = {0x01, 0x01, 0x01, 0x01, 0x01};
    TEST_ASSERT_EQ(
        (size_t)(sizeof(expected) / sizeof(uint8_t)),
        uint8_array_len(out)
    );

    for (size_t idx = 0; idx < sizeof(expected) / sizeof(uint8_t); idx++) {
        uint8_t value;
        TEST_ASSERT(uint8_array_get(out, idx, &value));
        TEST_ASSERT_EQ(
            expected[idx],
            value,
            "Decoded value at idx %zu was incorrect",
            idx
        );
    }

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_deflate_fixed_overlapping_alt) {
    uint8_t stream[] = {0xeb, 0xeb, 0x07, 0x43, 0x00};
    BitStream bitstream =
        bitstream_new(stream, sizeof(stream) / sizeof(uint8_t));

    Arena* arena = arena_new(1024);
    Uint8Array* out = NULL;
    TEST_PDF_REQUIRE(decode_deflate_data(arena, &bitstream, &out));

    uint8_t expected[] = {0x8e, 0x8f, 0x8e, 0x8f, 0x8e, 0x8f, 0x8e};
    TEST_ASSERT_EQ(
        (size_t)(sizeof(expected) / sizeof(uint8_t)),
        uint8_array_len(out)
    );

    for (size_t idx = 0; idx < sizeof(expected) / sizeof(uint8_t); idx++) {
        uint8_t value;
        TEST_ASSERT(uint8_array_get(out, idx, &value));
        TEST_ASSERT_EQ(
            expected[idx],
            value,
            "Decoded value at idx %zu was incorrect",
            idx
        );
    }

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_deflate_dyn_empty) {
    uint8_t stream[] =
        {0x05, 0xe1, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0xf8, 0xaf, 0x46
        };
    BitStream bitstream =
        bitstream_new(stream, sizeof(stream) / sizeof(uint8_t));

    Arena* arena = arena_new(1024);
    Uint8Array* out = NULL;
    TEST_PDF_REQUIRE(decode_deflate_data(arena, &bitstream, &out));

    TEST_ASSERT_EQ((size_t)0, uint8_array_len(out));

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_deflate_dyn_empty_no_dist) {
    uint8_t stream[] =
        {0x05, 0xc0, 0x81, 0x08, 0x00, 0x00, 0x00, 0x00, 0x20, 0x7f, 0xea, 0x2f
        };
    BitStream bitstream =
        bitstream_new(stream, sizeof(stream) / sizeof(uint8_t));

    Arena* arena = arena_new(1024);
    Uint8Array* out = NULL;
    TEST_PDF_REQUIRE(decode_deflate_data(arena, &bitstream, &out));

    TEST_ASSERT_EQ((size_t)0, uint8_array_len(out));

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_deflate_dyn_one_dist) {
    uint8_t stream[] = {
        0x0d,
        0xc0,
        0x01,
        0x09,
        0x00,
        0x00,
        0x00,
        0x80,
        0x20,
        0xfa,
        0x7f,
        0xda,
        0x6c,
        0x00
    };
    BitStream bitstream =
        bitstream_new(stream, sizeof(stream) / sizeof(uint8_t));

    Arena* arena = arena_new(1024);
    Uint8Array* out = NULL;
    TEST_PDF_REQUIRE(decode_deflate_data(arena, &bitstream, &out));

    uint8_t expected[] = {0x01, 0x01, 0x01, 0x01};
    TEST_ASSERT_EQ(
        (size_t)(sizeof(expected) / sizeof(uint8_t)),
        uint8_array_len(out)
    );

    for (size_t idx = 0; idx < sizeof(expected) / sizeof(uint8_t); idx++) {
        uint8_t value;
        TEST_ASSERT(uint8_array_get(out, idx, &value));
        TEST_ASSERT_EQ(
            expected[idx],
            value,
            "Decoded value at idx %zu was incorrect",
            idx
        );
    }

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_deflate_dynamic) {
    size_t base_len = 6007;
    size_t repeats = 2;
    uint8_t stream[] = {
        0xed, 0xd0, 0xd7, 0x22, 0x10, 0x00, 0x00, 0x05, 0x50, 0x33, 0x2b, 0x19,
        0x99, 0x19, 0x95, 0x99, 0x91, 0xec, 0x19, 0xd2, 0x90, 0x2d, 0xd9, 0x25,
        0x7b, 0x95, 0x11, 0x42, 0xc9, 0x2e, 0x23, 0x2a, 0x64, 0x37, 0xac, 0x8c,
        0x34, 0x6c, 0x22, 0x65, 0x66, 0x95, 0xad, 0x69, 0x94, 0x6c, 0x59, 0x0d,
        0x7b, 0xfb, 0x09, 0x8f, 0xf7, 0x13, 0xce, 0x21, 0xe3, 0x3b, 0x63, 0x7b,
        0xfb, 0x69, 0xdd, 0xaf, 0x6d, 0x4e, 0x45, 0x33, 0xdf, 0x47, 0x6f, 0xbe,
        0x2f, 0x33, 0x49, 0x19, 0x5c, 0x8b, 0x2d, 0xea, 0xfa, 0xb3, 0xef, 0xa8,
        0x96, 0x53, 0xe4, 0xf3, 0x96, 0x89, 0x3d, 0xbc, 0xa7, 0x6d, 0x6e, 0x65,
        0xd4, 0x0e, 0x6e, 0x71, 0x1c, 0xbf, 0x78, 0xf3, 0x61, 0xc5, 0xb7, 0x25,
        0x46, 0x49, 0x7d, 0x8f, 0x07, 0x85, 0x9d, 0x73, 0xd4, 0xc2, 0x9a, 0x57,
        0x22, 0x72, 0x9b, 0xc7, 0x49, 0x79, 0x4e, 0x59, 0x07, 0xa7, 0xd7, 0xfc,
        0xdc, 0x64, 0x57, 0xb8, 0xe0, 0x93, 0x5c, 0xfe, 0x75, 0x91, 0x41, 0xe2,
        0xbc, 0x7b, 0x4c, 0x41, 0xc7, 0xec, 0x5e, 0x21, 0x8d, 0xcb, 0x77, 0x9e,
        0x35, 0x8d, 0x91, 0x70, 0x9f, 0xb4, 0x0a, 0x4a, 0xab, 0xfe, 0xb1, 0xc1,
        0x26, 0x6f, 0x7a, 0x23, 0xe9, 0xf5, 0x97, 0x85, 0xfd, 0xe2, 0x7a, 0x6e,
        0xd1, 0xf9, 0xed, 0x33, 0x54, 0x82, 0xea, 0x8e, 0xe1, 0x39, 0x8d, 0xa3,
        0xc4, 0x5c, 0x2a, 0x96, 0x81, 0xa9, 0x55, 0x03, 0xeb, 0x07, 0xe4, 0x4c,
        0xae, 0x27, 0x96, 0x7d, 0x9e, 0xa7, 0x17, 0x3b, 0x77, 0x35, 0x2a, 0xaf,
        0x6d, 0x9a, 0x52, 0x40, 0xcd, 0x21, 0x2c, 0xbb, 0x61, 0x84, 0xe8, 0xf0,
        0x09, 0x8b, 0x80, 0x94, 0x77, 0xfd, 0x6b, 0xac, 0xb2, 0xc6, 0xde, 0x09,
        0xa5, 0x9f, 0xfe, 0xd3, 0x89, 0xea, 0xba, 0xde, 0x7f, 0xd5, 0x3a, 0x45,
        0x71, 0xe4, 0xac, 0x7d, 0x68, 0xd6, 0xfb, 0x61, 0xc2, 0x43, 0xca, 0xe6,
        0xfe, 0x4f, 0xde, 0xf6, 0xad, 0xb2, 0xc8, 0x18, 0x79, 0xc5, 0x97, 0xf4,
        0xfc, 0xa3, 0x3d, 0xa6, 0xe3, 0x72, 0xef, 0xe5, 0xc7, 0xdf, 0xe4, 0xfc,
        0xaa, 0x76, 0x21, 0x99, 0xf5, 0x43, 0x04, 0x07, 0x95, 0x2e, 0xf9, 0x3d,
        0xae, 0xec, 0x5d, 0x61, 0x96, 0x36, 0xf4, 0x8c, 0x2b, 0xee, 0xfe, 0x4b,
        0x23, 0xa2, 0xed, 0x7c, 0xf7, 0xc5, 0x87, 0x49, 0x32, 0xf8, 0xe1, 0x87,
        0x1f, 0x7e, 0xf8, 0xe1, 0x87, 0x1f, 0x7e, 0xf8, 0xe1, 0x87, 0x1f, 0x7e,
        0xf8, 0xe1, 0x87, 0x1f, 0x7e, 0xf8, 0xe1, 0x87, 0x1f, 0x7e, 0xf8, 0xe1,
        0xdf, 0x3d, 0x3f, 0xda, 0xe1, 0x87, 0x1f, 0x7e, 0xf8, 0xe1, 0x87, 0x1f,
        0x7e, 0xf8, 0xe1, 0x87, 0x1f, 0x7e, 0xf8, 0xe1, 0x87, 0x1f, 0x7e, 0xf8,
        0xe1, 0x87, 0x1f, 0x7e, 0xf8, 0xe1, 0x87, 0x7f, 0xd7, 0xfc, 0x3b
    };
    BitStream bitstream =
        bitstream_new(stream, sizeof(stream) / sizeof(uint8_t));

    Arena* arena = arena_new(1024);
    Uint8Array* out = NULL;
    TEST_PDF_REQUIRE(decode_deflate_data(arena, &bitstream, &out));

    uint8_t expected[base_len * repeats];
    for (size_t idx = 0; idx < base_len; idx++) {
        unsigned char val = (unsigned char)((idx * 31 + 7) & 0xFF);
        for (size_t repeat = 0; repeat < repeats; repeat++) {
            expected[repeat * base_len + idx] = val;
        }
    }

    TEST_ASSERT_EQ(
        (size_t)(sizeof(expected) / sizeof(uint8_t)),
        uint8_array_len(out)
    );

    for (size_t idx = 0; idx < sizeof(expected) / sizeof(uint8_t); idx++) {
        uint8_t value;
        TEST_ASSERT(uint8_array_get(out, idx, &value));
        TEST_ASSERT_EQ(
            expected[idx],
            value,
            "Decoded value at idx %zu was incorrect",
            idx
        );
    }

    arena_free(arena);
    return TEST_RESULT_PASS;
}

#endif
