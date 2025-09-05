#pragma once

#include <stdint.h>
#include <stdlib.h>

#include "arena/arena.h"
#include "arena/common.h"
#include "pdf_error/error.h"

/// Decodes data compressed in a zlib stream
PdfError* decode_zlib_data(
    Arena* arena,
    uint8_t* data,
    size_t data_len,
    Uint8Array** decoded_bytes
);
