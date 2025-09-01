#pragma once

#include "arena/arena.h"
#include "pdf/deserde_types.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"

typedef struct {
    PdfOpDict font;

    PdfObject* raw_dict;
} PdfResources;

PdfError* pdf_deserialize_resources(
    PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    PdfResources* deserialized
);

PdfError* pdf_deserialize_resources_wrapper(
    PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    void* deserialized
);

DESERIALIZABLE_OPTIONAL_TYPE(PdfOpResources, PdfResources)
