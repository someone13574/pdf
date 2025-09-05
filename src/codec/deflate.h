#pragma once

#include "arena/arena.h"
#include "arena/common.h"
#include "bitstream.h"
#include "pdf_error/error.h"

/// Decodes data compressed in a raw DEFLATE stream
PdfError* decode_deflate_data(
    Arena* arena,
    BitStream* bitstream,
    Uint8Array** decoded_bytes
);
