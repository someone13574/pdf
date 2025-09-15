#pragma once

#include "pdf/catalog.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"

typedef struct {
    /// The total number of entries in the file’s cross-reference table, as
    /// defined by the combination of the original section and all update
    /// sections. Equivalently, this value shall be 1 greater than the highest
    /// object number defined in the file. Any object in a cross-reference
    /// section whose number is greater than this value shall be ignored and
    /// defined to be missing by a conforming reader.
    PdfInteger size;

    /// The catalog dictionary for the PDF document contained in the file.
    PdfCatalogRef root;

    /// The document’s information dictionary.
    PdfOpDict info;

    /// An array of two byte-strings constituting a file identifier (see 14.4,
    /// "File Identifiers") for the file. If there is an Encrypt entry this
    /// array and the two byte-strings shall be direct objects and shall be
    /// unencrypted.
    PdfOpArray id;

    const PdfObject* raw_dict;
} PdfTrailer;

PdfError* pdf_deserialize_trailer(
    const PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    PdfTrailer* deserialized
);
