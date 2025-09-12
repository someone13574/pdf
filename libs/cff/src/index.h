#pragma once

#include "arena/arena.h"
#include "parser.h"
#include "pdf_error/error.h"
#include "types.h"

#define DARRAY_NAME CffOffsetArray
#define DARRAY_LOWERCASE_NAME cff_offset_array
#define DARRAY_TYPE CffOffset
#include "arena/darray_decl.h"

#define DARRAY_NAME CffCard8Array
#define DARRAY_LOWERCASE_NAME cff_card8_array
#define DARRAY_TYPE CffCard8
#include "arena/darray_decl.h"

/// An INDEX is an array of variable-sized objects. It comprises a header, an
/// offset array, and object data. The offset array specifies offsets within the
/// object data. An object is retrieved by indexing the offset array and
/// fetching the object at the specified offset. The object’s length can be
/// determined by subtracting its offset from the next offset in the offset
/// array. An additional offset is added at the end of the offset array so the
/// length of the last object may be determined.
typedef struct {
    /// Number of objects stored in INDEX.
    CffCard16 count;

    /// Offset array element size.
    CffOffsetSize offset_size;

    /// Offset array (from byte preceding object data).
    CffOffsetArray* offset;

    /// Object data.
    CffCard8Array* data;
} CffIndex;

/// Read a `CffIndex` from the current parser offset.
PdfError* cff_parse_index(CffParser* parser, Arena* arena, CffIndex* index_out);
