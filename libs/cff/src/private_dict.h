#pragma once

#include <stdbool.h>

#include "arena/arena.h"
#include "arena/common.h"
#include "err/error.h"
#include "parse_ctx/ctx.h"
#include "types.h"

typedef struct {
    Int32Array* blue_values;
    Int32Array* other_blues;
    Int32Array* blue_blues;
    Int32Array* family_other_blues;
    CffNumber blue_scale;
    CffNumber blue_shift;
    CffNumber blue_fuzz;
    CffNumber std_hw;
    CffNumber std_vw;
    Int32Array* stem_snap_h;
    Int32Array* stem_snap_v;
    bool force_bold;
    int32_t language_group;
    CffNumber expansion_factor;
    int32_t initial_random_seed;
    int32_t subrs;
    CffNumber default_width_x;
    CffNumber nominal_width_x;
} CffPrivateDict;

Error* cff_parse_private_dict(
    Arena* arena,
    ParseCtx* ctx,
    size_t length,
    CffPrivateDict* out
);

CffPrivateDict cff_private_dict_default(void);
