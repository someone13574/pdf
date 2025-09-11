#pragma once

#include "arena/arena.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"

typedef struct {
    /// A string identifying the issuer of the character collection.
    PdfString registry;

    /// A string that uniquely names the character collection within the
    /// specified registry.
    PdfString ordering;

    /// The supplement number of the character collection. An original character
    /// collection has a supplement number of 0. Whenever additional CIDs are
    /// assigned in a character collection, the supplement number shall be
    /// increased. Supplements shall not alter the ordering of existing CIDs in
    /// the character collection. This value shall not be used in determining
    /// compatibility between character collections.
    PdfInteger supplement;

    const PdfObject* raw_dict;
} PdfCIDSystemInfo;

PdfError* pdf_deserialize_cid_system_info(
    const PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    PdfCIDSystemInfo* deserialized
);

PdfError* pdf_deserialize_cid_system_info_wrapper(
    const PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    void* deserialized
);

typedef struct PdfCMap PdfCMap;

PdfError*
pdf_cmap_get_cid(PdfCMap* cmap, uint32_t codepoint, uint32_t* cid_out);

PdfError* pdf_parse_cmap(
    Arena* arena,
    const char* data,
    size_t data_len,
    PdfCMap** cmap_out
);

PdfError* pdf_load_cmap(Arena* arena, char* name, PdfCMap** cmap_out);

typedef struct PdfCMapCache PdfCMapCache;

/// Creates a new cmap cache.
PdfCMapCache* pdf_cmap_cache_new(Arena* arena);

/// Gets a cmap associated with a name, loading it if required.
PdfError*
pdf_cmap_cache_get(PdfCMapCache* cache, char* name, PdfCMap** cmap_out);
