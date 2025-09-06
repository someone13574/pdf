#pragma once

#include <stdint.h>
#include <stdlib.h>

/// An Adler-32 checksum as defined by RFC-1950
typedef uint32_t Adler32Sum;

/// Computes the Alder-32 checksum of buffer
Adler32Sum adler32_compute_checksum(const uint8_t* data, size_t data_len);

/// Swaps the endianness of a checksum
void adler32_swap_endianness(Adler32Sum* sum);
