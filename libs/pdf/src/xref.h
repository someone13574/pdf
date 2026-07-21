#pragma once

#include "ctx.h"
#include "err/error.h"
#include "pdf/object.h"

typedef struct XRefTable XRefTable;

typedef enum {
    XREF_ENTRY_FREE,
    XREF_ENTRY_IN_USE,
    XREF_ENTRY_COMPRESSED
} XRefEntryType;

typedef struct {
    XRefEntryType type;

    /// In-use: the byte offset of the object. Free: the object number of the
    /// next free object
    size_t offset;
    size_t generation;

    /// Compressed: the object stream holding this object, and the index of
    /// this object within it
    size_t stream_object_id;
    size_t stream_index;

    bool entry_parsed;
    PdfObject* object;
} XRefEntry;

XRefTable* pdf_xref_init(Arena* arena, PdfCtx* ctx);

/// Parses a section and its trailer dict (the stream dict for xref streams)
Error* pdf_xref_parse_section(
    PdfResolver* resolver,
    size_t xrefstart,
    XRefTable* xref,
    PdfObject* trailer_dict
);

Error* pdf_xref_get_entry(
    XRefTable* xref,
    size_t object_id,
    size_t generation,
    XRefEntry** entry
);
