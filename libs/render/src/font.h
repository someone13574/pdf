#pragma once

#include <stdint.h>

#include "pdf/fonts/font.h"

/// Returns the next CID in the data stream, advancing `offset` and setting
/// `finished` if this is the last CID in the stream.
uint32_t
next_cid(PdfFont* font, PdfString* data, size_t* offset, bool* finished);
