#pragma once

#include <stdint.h>

#include "err/error.h"
#include "parse_ctx/ctx.h"

Error*
sfnt_validate_checksum(ParseCtx ctx, uint32_t checksum, bool is_head_table);
