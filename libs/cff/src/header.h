#pragma once

#include "err/error.h"
#include "parser.h"
#include "types.h"

typedef struct {
    /// Format major version (starting at 1)
    CffCard8 major;

    /// Format minor version (starting at 0)
    CffCard8 minor;

    /// Header size (bytes)
    CffCard8 header_size;

    /// Absolute offset (0) size
    CffOffsetSize offset_size;
} CffHeader;

Error* cff_read_header(CffParser* parser, CffHeader* cff_header_out);
