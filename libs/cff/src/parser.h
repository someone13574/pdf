#pragma once

#include <stdint.h>
#include <stdlib.h>

#include "arena/arena.h"
#include "err/error.h"
#include "types.h"

/// Helper-struct for parsing CFF files.
typedef struct {
    const uint8_t* buffer;
    size_t buffer_len;
    size_t offset;
} CffParser;

/// Create a new CffParser.
CffParser cff_parser_new(const uint8_t* buffer, size_t buffer_len);

/// Change the current byte-offset of the parser.
Error* cff_parser_seek(CffParser* parser, size_t offset);

/// Read a 8-bit unsigned integer at the current offset.
Error* cff_parser_read_card8(CffParser* parser, CffCard8* card8_out);

/// Read a 16-bit unsigned integer at the current offset.
Error* cff_parser_read_card16(CffParser* parser, CffCard16* card16_out);

/// Read a 1-4 byte unsigned integer from the current offset.
Error* cff_parser_read_offset(
    CffParser* parser,
    CffOffsetSize offset_size,
    CffOffset* offset_out
);

/// Read an 8-bit unsigned integer from the current offset which must have a
/// value from 1-4.
Error*
cff_parser_read_offset_size(CffParser* parser, CffOffsetSize* offset_size_out);

/// Read a 2-byte string identifier which must have a value from 0 to 64999 from
/// the current offset.
Error* cff_parser_read_sid(CffParser* parser, CffSID* sid_out);

/// Read a operator or operand from the current offset.
Error* cff_parser_read_token(CffParser* parser, CffToken* token_out);

/// Get a null-terminated string of a specified length from the current offset.
Error* cff_parser_get_str(
    Arena* arena,
    CffParser* parser,
    size_t length,
    char** str_out
);
