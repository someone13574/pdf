#pragma once

#include <stddef.h>
#include <stdint.h>

#include "arena/common.h"
#include "pdf_error/error.h"
#include "sfnt/types.h"

typedef struct {
    const uint8_t* buffer;
    size_t buffer_len;
    size_t offset;
} SfntParser;

void sfnt_parser_new(
    const uint8_t* buffer,
    size_t buffer_len,
    SfntParser* parser
);
PdfError* sfnt_subparser_new(
    SfntParser* parent,
    uint32_t offset,
    uint32_t len,
    uint32_t checksum,
    bool head_table,
    SfntParser* parser
);

PdfError* sfnt_parser_seek(SfntParser* parser, size_t offset);

PdfError* sfnt_parser_read_int8(SfntParser* parser, int8_t* out);
PdfError* sfnt_parser_read_uint8(SfntParser* parser, uint8_t* out);
PdfError* sfnt_parser_read_int16(SfntParser* parser, int16_t* out);
PdfError* sfnt_parser_read_uint16(SfntParser* parser, uint16_t* out);
PdfError* sfnt_parser_read_int32(SfntParser* parser, int32_t* out);
PdfError* sfnt_parser_read_uint32(SfntParser* parser, uint32_t* out);
PdfError* sfnt_parser_read_int64(SfntParser* parser, int64_t* out);
PdfError* sfnt_parser_read_uint64(SfntParser* parser, uint64_t* out);

PdfError* sfnt_parser_read_uint8_array(SfntParser* parser, Uint8Array* array);
PdfError* sfnt_parser_read_uint16_array(SfntParser* parser, Uint16Array* array);

PdfError* sfnt_parser_read_short_frac(SfntParser* parser, SfntShortFrac* out);
PdfError* sfnt_parser_read_fixed(SfntParser* parser, SfntFixed* out);
PdfError* sfnt_parser_read_fword(SfntParser* parser, SfntFWord* out);
PdfError* sfnt_parser_read_ufword(SfntParser* parser, SfntUFWord* out);
PdfError*
sfnt_parser_read_long_date_time(SfntParser* parser, SfntLongDateTime* out);
