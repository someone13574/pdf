#pragma once

#include <stdint.h>
#include <stdlib.h>

#include "arena/arena.h"
#include "arena/common.h"
#include "pdf_error/error.h"

/// Decodes data compressed in a zlib stream. This function sets the value of
/// `decoded_bytes`
PdfError* decode_zlib_data(
    Arena* arena,
    const uint8_t* data,
    size_t data_len,
    Uint8Array** decoded_bytes
);
