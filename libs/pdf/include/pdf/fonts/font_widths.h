#pragma once

#include "arena/arena.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"

typedef struct {
    /// Whether this entry has been actually set. If this is false, the default
    /// width must be used.
    bool has_value;

    /// The horizontal displacement between the origin of the glyph and the
    /// origin of the next glyph when writing in horizontal mode. If `has_value`
    /// is false, this value is invalid.
    PdfInteger width;
} PdfFontWidthEntry;

#define DVEC_NAME PdfFontWidthVec
#define DVEC_LOWERCASE_NAME pdf_font_width_vec
#define DVEC_TYPE PdfFontWidthEntry
#include "arena/dvec_decl.h"

typedef struct {
    /// Lookup table for widths by CID
    PdfFontWidthVec* cid_to_width;

    const PdfObject* raw_dict;
} PdfFontWidths;

typedef struct {
    bool discriminant;
    PdfFontWidths value;
} PdfOpFontWidths;

PdfError* pdf_deserialize_font_widths(
    const PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    PdfFontWidths* deserialized
);

PdfError* pdf_deserialize_font_widths_wrapper(
    const PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    void* deserialized
);
