#pragma once

#include <stdint.h>
#include <stdlib.h>

#include "err/error.h"

typedef struct {
    const uint8_t* data;
    size_t length_bytes;

    size_t offset;
} BitStream;

/// Create a new bitstream
BitStream bitstream_new(const uint8_t* data, size_t n_bytes);

/// Read the next bit in the bitstream
Error* bitstream_next(BitStream* bitstream, uint32_t* out);

/// Read the next N bits in the bitstream (maximum 32 bits)
Error* bitstream_read_n(BitStream* bitstream, size_t n_bits, uint32_t* out);

/// Skips remaining bits in the current byte if it is partially consumed
void bitstream_align_byte(BitStream* bitstream);

/// Get the remaining bits in the bitstream
size_t bitstream_remaining_bits(const BitStream* bitstream);
