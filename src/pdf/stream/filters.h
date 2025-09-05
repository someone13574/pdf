#pragma once

#include <stdlib.h>

#include "arena/arena.h"
#include "pdf_error/error.h"
#include "stream_dict.h"

PdfError* pdf_decode_filtered_stream(
    Arena* arena,
    const char* encoded,
    size_t length,
    PdfOpNameArray filters,
    char** decoded,
    size_t* decoded_len
);

PdfError* pdf_filter_ascii_hex_decode(
    Arena* arena,
    const uint8_t* stream,
    size_t stream_len,
    uint8_t** decoded,
    size_t* decoded_len
);
