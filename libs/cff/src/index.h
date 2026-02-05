#pragma once

#include "err/error.h"
#include "parse_ctx/ctx.h"
#include "types.h"

/// An INDEX is an array of variable-sized objects. It comprises a header, an
/// offset array, and object data. The offset array specifies offsets within the
/// object data. An object is retrieved by indexing the offset array and
/// fetching the object at the specified offset. The object’s length can be
/// determined by subtracting its offset from the next offset in the offset
/// array. An additional offset is added at the end of the offset array so the
/// length of the last object may be determined.
typedef struct {
    /// Number of objects stored in INDEX.
    uint16_t count;

    /// Offset array element size.
    CffOffsetSize offset_size;

    /// Parser offset of index.
    size_t parser_start_offset;
} CffIndex;

/// Read a `CffIndex` from the current parser offset.
Error* cff_parse_index(ParseCtx* ctx, CffIndex* index_out);

/// Seeks to the end of a CFF index.
Error* cff_index_skip(CffIndex* index, ParseCtx* ctx);

/// Seeks to object `object_idx` and returns its size.
Error* cff_index_seek_object(
    CffIndex* index,
    ParseCtx* ctx,
    uint16_t object_idx,
    size_t* object_size_out
);
