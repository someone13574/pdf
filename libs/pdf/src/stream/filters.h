#pragma once

#include <stdlib.h>

#include "arena/arena.h"
#include "err/error.h"
#include "pdf/types.h"

Error* pdf_decode_filtered_stream(
    Arena* arena,
    const uint8_t* encoded,
    size_t length,
    PdfAsNameVecOptional filters,
    uint8_t** decoded,
    size_t* decoded_len
);

Error* pdf_filter_ascii_hex_decode(
    Arena* arena,
    const uint8_t* stream,
    size_t stream_len,
    uint8_t** decoded,
    size_t* decoded_len
);
