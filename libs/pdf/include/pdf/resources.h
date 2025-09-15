#pragma once

#include "arena/arena.h"
#include "pdf/deserde_types.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf/stream/stream_dict.h"
#include "pdf_error/error.h"

// TODO: Use typed lazy refs
typedef struct {
    /// A dictionary that maps resource names to external objects.
    PdfOpDict xobject;

    /// A dictionary that maps resource names to font dictionaries.
    PdfOpDict font;

    /// An array of predefined procedure set names.
    PdfOpNameArray proc_set;

    const PdfObject* raw_dict;
} PdfResources;

PdfError* pdf_deserialize_resources(
    const PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    PdfResources* deserialized
);

PdfError* pdf_deserialize_resources_wrapper(
    const PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    void* deserialized
);

DESERIALIZABLE_OPTIONAL_TYPE(PdfOpResources, PdfResources)
