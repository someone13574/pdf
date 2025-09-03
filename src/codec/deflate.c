#include "codec/deflate.h"

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
        uint32_t reversed_code = reverse_bits(code, (int)max_bit_len);

        RELEASE_ASSERT(reversed_code < lut.lut_length);

        lut.symbol_lut[reversed_code] = (uint32_t)symbol_idx;
        lut.length_lut[reversed_code] = bits;
        next_code[bits]++;
    }

    // Fill out empty entries
    for (uint32_t reversed_code = 0; reversed_code < lut.lut_length;
         reversed_code++) {
        if (lut.length_lut[reversed_code] != 0) {
            continue;
        }

        // Find shortest valid code
        for (size_t bits = min_bit_len; bits < max_bit_len; bits++) {
            uint32_t mask = (uint32_t)(1 << (max_bit_len - bits)) - 1;
            uint32_t masked_reversed_code = reversed_code & ~mask;

            RELEASE_ASSERT(masked_reversed_code < lut.lut_length);

            if (lut.length_lut[masked_reversed_code] == (uint32_t)bits) {
                lut.symbol_lut[reversed_code] =
                    lut.symbol_lut[masked_reversed_code];
                lut.length_lut[reversed_code] =
                    lut.length_lut[masked_reversed_code];
                break;
            }
        }
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
        uint32_t encoded_symbol;
        PDF_PROPAGATE(
            bitstream_read_n(bitstream, lut->max_bit_len, &encoded_symbol)
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

    uint32_t buffer;
    PDF_PROPAGATE(bitstream_read_n(bitstream, bits, &buffer));

    do {
        uint32_t symbol = lut->symbol_lut[buffer];
        uint32_t symbol_len = lut->length_lut[buffer];

        if (symbol_len == bits) {
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

    uint32_t num_lit_code_lens;
    PDF_PROPAGATE(bitstream_read_n(bitstream, 5, &num_lit_code_lens));
    num_lit_code_lens += 257;

    uint32_t num_dist_code_lens;
    PDF_PROPAGATE(bitstream_read_n(bitstream, 5, &num_dist_code_lens));
    num_dist_code_lens += 1;

    uint32_t num_code_len_symbols;
    PDF_PROPAGATE(bitstream_read_n(bitstream, 4, &num_code_len_symbols));
    num_code_len_symbols += 4;

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

    while (bit_len_offset < num_lit_code_lens + num_dist_code_lens) {
        uint32_t symbol;
        PDF_PROPAGATE(deflate_huffman_decode(bitstream, &code_len_lut, &symbol)
        );

        if (symbol <= 15) {
            bit_lens[bit_len_offset++] = (uint8_t)symbol;
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
        initialized = true;

        size_t offset = 0;
        uint8_t bit_lens[288];

        uint8_t region_vals[] = {8, 9, 7, 8};
        uint32_t region_lens[] = {144, 112, 24, 8};
        for (size_t region_idx = 0; region_idx < 4; region_idx++) {
            for (size_t idx = 0; idx < region_lens[region_idx]; idx++) {
                bit_lens[offset++] = region_vals[region_idx];
            }
        }

        Arena* persistent_arena = arena_new(1); // this leaks, and that's ok
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
    }

    return NULL;
}

static PdfError* deflate_decode_distance_code(
    BitStream* bitstream,
    DeflateHuffmanLut* dist_lut,
    uint32_t* distance
) {
    RELEASE_ASSERT(bitstream);
    RELEASE_ASSERT(distance);

    // Get distance symbol
    uint32_t dist_symbol = 0;
    if (dist_lut) {
        PDF_PROPAGATE(deflate_huffman_decode(bitstream, dist_lut, &dist_symbol)
        );
    } else {
        PDF_PROPAGATE(bitstream_read_n(bitstream, 5, &dist_symbol));
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

    return NULL;
}

PdfError* decode_deflate_data(
    Arena* arena,
    uint8_t* data,
    size_t data_len,
    Uint8Array** decoded_bytes
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(data);
    RELEASE_ASSERT(decoded_bytes);

    Arena* block_arena = arena_new(1024);
    Arena* decode_arena = arena_new(1024);

    BitStream bitstream = bitstream_new(data, data_len);
    Uint8Vec* output_stream = uint8_vec_new(decode_arena);

    DeflateBlockHeader header;
    do {
        PDF_PROPAGATE(deflate_parse_block_header(&bitstream, &header));

        if (header.type == DEFLATE_BLOCK_COMPRESSION_NONE) {
            bitstream_align_byte(&bitstream);

            uint32_t len;
            uint32_t n_len;
            PDF_PROPAGATE(bitstream_read_n(&bitstream, 16, &len));
            PDF_PROPAGATE(bitstream_read_n(&bitstream, 16, &n_len));

            if ((uint16_t)len != (uint16_t)~n_len) {
                return PDF_ERROR(
                    PDF_ERR_DEFLATE_LEN_COMPLIMENT,
                    "Uncompressed block length's one's compliment didn't match"
                );
            }

            for (size_t idx = 0; idx < len; idx++) {
                uint32_t byte;
                PDF_PROPAGATE(bitstream_read_n(&bitstream, 8, &byte));

                uint8_vec_push(output_stream, (uint8_t)byte);
            }
        } else {
            DeflateHuffmanLut lit_lut;
            DeflateHuffmanLut dist_lut; // only used for dynamic blocks

            if (header.type == DEFLATE_BLOCK_COMPRESSION_DYN) {
                PDF_PROPAGATE(decode_dyn_huffman_table_luts(
                    block_arena,
                    &bitstream,
                    &lit_lut,
                    &dist_lut
                ));
            } else {
                lit_lut = get_fixed_huffman_lut();
            }

            do {
                uint32_t lit_symbol;
                PDF_PROPAGATE(
                    deflate_huffman_decode(&bitstream, &lit_lut, &lit_symbol)
                );

                if (lit_symbol < 256) {
                    uint8_vec_push(output_stream, (uint8_t)lit_symbol);
                } else if (lit_symbol == 256) {
                    break;
                } else {
                    uint32_t length;
                    PDF_PROPAGATE(deflate_decode_length_code(
                        &bitstream,
                        lit_symbol,
                        &length
                    ));

                    uint32_t distance;
                    PDF_PROPAGATE(deflate_decode_distance_code(
                        &bitstream,
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

TEST_FUNC(test_deflate_huffman) {
    uint8_t bit_lengths[] = {3, 3, 3, 3, 3, 2, 4, 4};

    Arena* lut_arena = arena_new(1024);
    DeflateHuffmanLut lut =
        build_deflate_huffman_lut(lut_arena, bit_lengths, 8);

    TEST_ASSERT_EQ((size_t)16, lut.lut_length);
    TEST_ASSERT_EQ((size_t)2, lut.min_bit_len);
    TEST_ASSERT_EQ((size_t)4, lut.max_bit_len);

    // -1 in expected_symbols means there is no valid truncated form
    uint8_t expected_len[16] = {2, 2, 3, 2, 3, 3, 3, 4, 0, 0, 3, 3, 3, 3, 0, 4};
    int expected_symbols[16] =
        {5, 5, 2, 5, 0, 0, 4, 6, -1, -1, 3, 3, 1, 1, -1, 7};

    for (size_t idx = 0; idx < 16; idx++) {
        TEST_ASSERT_EQ(
            expected_len[idx],
            lut.length_lut[idx],
            "Length mismatch for reverse code idx %zu",
            idx
        );

        TEST_ASSERT_EQ(
            expected_symbols[idx],
            lut.length_lut[idx] == 0 ? -1 : (int)lut.symbol_lut[idx],
            "Symbol mismatch for reverse code idx %zu",
            idx
        );
    }

    arena_free(lut_arena);
    return TEST_RESULT_PASS;
}

#endif
