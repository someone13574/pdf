#pragma once

#include "arena/arena.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"

// TODO: Use typed lazy refs
typedef struct {
    /// (Optional) A dictionary that maps resource names to graphics state
    /// parameter dictionaries (see 8.4.5, "Graphics State Parameter
    /// Dictionaries").
    PdfDictOptional ext_gstate;

    /// (Optional) A dictionary that maps each resource name to either the name
    /// of a device-dependent colour space or an array describing a colour space
    /// (see 8.6, "Colour Spaces").
    PdfDictOptional color_space;

    /// (Optional) A dictionary that maps resource names to pattern objects
    /// (see 8.7, "Patterns").
    PdfDictOptional pattern;

    /// (Optional; PDF 1.3) A dictionary that maps resource names to shading
    /// dictionaries (see 8.7.4.3, "Shading Dictionaries").
    PdfDictOptional shading;

    /// (Optional) A dictionary that maps resource names to external objects
    /// (see 8.8, "External Objects").
    PdfDictOptional xobject;

    /// (Optional) A dictionary that maps resource names to font dictionaries
    /// see clause 9, "Text").
    PdfDictOptional font;

    /// (Optional) An array of predefined procedure set names (see 14.2,
    /// "Procedure Sets").
    PdfNameVecOptional proc_set;
} PdfResources;

PdfError* pdf_deserialize_resources(
    const PdfObject* object,
    PdfResources* target_ptr,
    PdfOptionalResolver resolver,
    Arena* arena
);

DESERDE_DECL_OPTIONAL(PdfResourcesOptional, PdfResources, pdf_resources_op_init)
DESERDE_DECL_TRAMPOLINE(pdf_deserialize_resources_trampoline)
