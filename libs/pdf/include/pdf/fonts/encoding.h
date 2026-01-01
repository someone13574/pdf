#pragma once

#include <stdint.h>

#include "pdf/object.h"
#include "pdf/resolver.h"

const char* pdf_decode_mac_roman_codepoint(uint8_t codepoint);
const char* pdf_decode_win_ansi_codepoint(uint8_t codepoint);
const char* pdf_decode_adobe_standard_codepoint(uint8_t codepoint);

typedef struct {
    PdfNameOptional type;
    PdfNameOptional base_encoding;
    PdfArrayOptional differences;
} PdfEncodingDict;

Error* pdf_deserde_encoding_dict(
    const PdfObject* object,
    PdfEncodingDict* target_ptr,
    PdfResolver* resolver
);

const char* pdf_encoding_map_codepoint(
    const PdfEncodingDict* encoding_dict,
    uint8_t codepoint
);

DESERDE_DECL_TRAMPOLINE(pdf_deserde_encoding_dict_trampoline)
DESERDE_DECL_OPTIONAL(
    PdfEncodingDictOptional,
    PdfEncodingDict,
    pdf_encoding_dict_op_init
)
