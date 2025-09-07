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
    PdfString supplement;

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

PdfError* pdf_parse_cmap(const uint8_t* data, size_t data_len);
