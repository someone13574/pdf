#pragma once

#include "parser.h"
#include "sfnt/sfnt.h"

PdfError* sfnt_parse_head(SfntParser* parser, SfntHead* head);
