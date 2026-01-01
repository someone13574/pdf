#pragma once

#include "parser.h"
#include "sfnt/sfnt.h"

Error* sfnt_parse_head(SfntParser* parser, SfntHead* head);
