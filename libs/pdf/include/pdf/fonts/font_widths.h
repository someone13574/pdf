#pragma once

#include "err/error.h"
#include "pdf/deserde.h"
#include "pdf/object.h"
#include "pdf/resolver.h"

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
} PdfFontWidths;

Error* pdf_deserde_font_widths(
    const PdfObject* object,
    PdfFontWidths* deserialized,
    PdfResolver* resolver
);

PDF_DECL_OPTIONAL_FIELD(PdfFontWidths, PdfFontWidthsOptional, font_widths)
