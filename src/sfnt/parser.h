#pragma once

#include <stddef.h>
#include <stdint.h>

#include "pdf/result.h"
#include "sfnt/cmap.h"

typedef struct {
    uint8_t* buffer;
    size_t buffer_len;
    size_t offset;
} SfntParser;

void sfnt_parser_new(uint8_t* buffer, size_t buffer_len, SfntParser* parser);
PdfResult sfnt_subparser_new(
    SfntParser* parent,
    uint32_t offset,
    uint32_t len,
    uint32_t checksum,
    SfntParser* parser
);

PdfResult sfnt_parser_read_uint16(SfntParser* parser, uint16_t* out);
PdfResult sfnt_parser_read_uint32(SfntParser* parser, uint32_t* out);

PdfResult
sfnt_parser_read_uint16_array(SfntParser* parser, SfntUint16Array* array);
