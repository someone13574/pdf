#pragma once

#include "err/error.h"
#include "pdf/object.h"
#include "pdf/resolver.h"

/// Parses the object numbered `object_id` stored at `stream_index` within an
/// object stream
Error* pdf_object_stream_get(
    PdfResolver* resolver,
    const PdfObject* stream_object,
    size_t stream_index,
    size_t object_id,
    PdfObject* object
);
