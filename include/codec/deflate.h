#pragma once

#include <stdint.h>
#include <stdlib.h>

#include "pdf_error/error.h"

/// Decodes data compressed in DEFLATE
PdfError* decode_deflate_data(uint8_t* data, size_t data_len);
