#pragma once

#include <stddef.h>
#include <stdint.h>

#include "pdf/result.h"
#include "sfnt/cmap.h"
#include "sfnt/types.h"

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
    bool head_table,
    SfntParser* parser
);

PdfResult sfnt_parser_read_int16(SfntParser* parser, int16_t* out);
PdfResult sfnt_parser_read_uint16(SfntParser* parser, uint16_t* out);
PdfResult sfnt_parser_read_int32(SfntParser* parser, int32_t* out);
PdfResult sfnt_parser_read_uint32(SfntParser* parser, uint32_t* out);
PdfResult sfnt_parser_read_int64(SfntParser* parser, int64_t* out);
PdfResult sfnt_parser_read_uint64(SfntParser* parser, uint64_t* out);

PdfResult
sfnt_parser_read_uint16_array(SfntParser* parser, SfntUint16Array* array);

PdfResult sfnt_parser_read_short_frac(SfntParser* parser, SfntShortFrac* out);
PdfResult sfnt_parser_read_fixed(SfntParser* parser, SfntFixed* out);
PdfResult sfnt_parser_read_fword(SfntParser* parser, SfntFWord* out);
PdfResult sfnt_parser_read_ufword(SfntParser* parser, SfntUFWord* out);
PdfResult
sfnt_parser_read_long_date_time(SfntParser* parser, SfntLongDateTime* out);
