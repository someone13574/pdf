#pragma once

#include <stdlib.h>

#include "arena/arena.h"
#include "stream_dict.h"

char* pdf_decode_filtered_stream(
    Arena* arena,
    const char* encoded,
    size_t length,
    PdfOpNameArray filters
);

PdfError* pdf_filter_ascii_hex_decode(
    Arena* arena,
    const char* stream,
    size_t stream_len,
    uint8_t** decoded,
    size_t* decoded_len
);
