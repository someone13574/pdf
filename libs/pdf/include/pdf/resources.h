#pragma once

#include "arena/arena.h"
#include "pdf/deserde_types.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"

// TODO: Use typed lazy refs
typedef struct {
    /// (Optional) A dictionary that maps resource names to graphics state
    /// parameter dictionaries (see 8.4.5, "Graphics State Parameter
    /// Dictionaries").
    PdfOpDict ext_gstate;

    /// (Optional) A dictionary that maps each resource name to either the name
    /// of a device-dependent colour space or an array describing a colour space
    /// (see 8.6, "Colour Spaces").
    PdfOpDict color_space;

    /// (Optional) A dictionary that maps resource names to pattern objects
    /// (see 8.7, "Patterns").
    PdfOpDict pattern;

    /// (Optional; PDF 1.3) A dictionary that maps resource names to shading
    /// dictionaries (see 8.7.4.3, "Shading Dictionaries").
    PdfOpDict shading;

    /// (Optional) A dictionary that maps resource names to external objects
    /// (see 8.8, "External Objects").
    PdfOpDict xobject;

    /// (Optional) A dictionary that maps resource names to font dictionaries
    /// see clause 9, "Text").
    PdfOpDict font;

    /// (Optional) An array of predefined procedure set names (see 14.2,
    /// "Procedure Sets").
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
