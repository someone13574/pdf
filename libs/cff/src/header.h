#pragma once

#include "err/error.h"
#include "parse_ctx/ctx.h"
#include "types.h"

typedef struct {
    /// Format major version (starting at 1)
    uint8_t major;

    /// Format minor version (starting at 0)
    uint8_t minor;

    /// Header size (bytes)
    uint8_t header_size;

    /// Absolute offset (0) size
    CffOffsetSize offset_size;
} CffHeader;

Error* cff_read_header(ParseCtx* parser, CffHeader* cff_header_out);
