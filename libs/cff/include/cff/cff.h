#pragma once

#include <stdint.h>
#include <stdlib.h>

#include "arena/arena.h"
#include "pdf_error/error.h"

/// A CFF Font.
typedef struct CffFont CffFont;

/// Parse a CFF Font, storing the result in `cff_font_out`.
PdfError* cff_parse(
    Arena* arena,
    const uint8_t* data,
    size_t data_len,
    CffFont** cff_font_out
);
