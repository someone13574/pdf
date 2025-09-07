#pragma once

#include "pdf/deserde_types.h"
#include "pdf/object.h"
#include "pdf/types.h"

typedef struct {
    /// The type of PDF object that this dictionary describes; shall be
    /// FontDescriptor for a font descriptor.
    PdfName type;

    /// The PostScript name of the font. This name shall be the same as the
    /// value of BaseFont in the font or CIDFont dictionary that refers to this
    /// font descriptor.
    PdfName font_name;

    /// A byte string specifying the preferred font family name.
    PdfOpName font_family;

    /// The font stretch value. It shall be one of these names (ordered from
    /// narrowest to widest): UltraCondensed, ExtraCondensed, Condensed,
    /// SemiCondensed, Normal, SemiExpanded, Expanded, ExtraExpanded or
    /// UltraExpanded. The specific interpretation of these values varies from
    /// font to font.
    PdfOpName font_stretch;

    /// The weight (thickness) component of the fully-qualified font name or
    /// font specifier. The possible values shall be 100, 200, 300, 400, 500,
    /// 600, 700, 800, or 900, where each number indicates a weight that is at
    /// least as dark as its predecessor. A value of 400 shall indicate a normal
    /// weight; 700 shall indicate bold.
    PdfOpNumber font_weight;

    /// A collection of flags defining various characteristics of the font.
    PdfInteger flags;

    /// A rectangle (see 7.9.5, "Rectangles"), expressed in the glyph coordinate
    /// system, that shall specify the font bounding box. This should be the
    /// smallest rectangle enclosing the shape that would result if all of the
    /// glyphs of the font were placed with their origins coincident and then
    /// filled.
    PdfOpRectangle font_bbox;

    /// The angle, expressed in degrees counterclockwise from the vertical, of
    /// the dominant vertical strokes of the font. The value shall be negative
    /// for fonts that slope to the right, as almost all italic fonts do.
    PdfNumber italic_angle;

    /// The maximum height above the baseline reached by glyphs in this font.
    /// The height of glyphs for accented characters shall be excluded.
    PdfOpNumber ascent;

    /// The maximum depth below the baseline reached by glyphs in this font. The
    /// value shall be a negative number.
    PdfOpNumber descent;

    /// The spacing between baselines of consecutive lines of text. Default
    /// value: 0.
    PdfOpNumber leading;

    /// The vertical coordinate of the top of flat capital letters, measured
    /// from the baseline.
    PdfOpNumber cap_height;

    /// The font’s x height: the vertical coordinate of the top of flat
    /// nonascending lowercase letters (like the letter x), measured from the
    /// baseline, in fonts that have Latin characters. Default value: 0.
    PdfOpNumber x_height;

    /// The thickness, measured horizontally, of the dominant vertical stems of
    /// glyphs in the font.
    PdfOpNumber stem_v;

    /// The thickness, measured vertically, of the dominant horizontal stems of
    /// glyphs in the font. Default value: 0.
    PdfOpNumber stem_h;

    /// The average width of glyphs in the font. Default value: 0.
    PdfOpNumber avg_width;

    /// The maximum width of glyphs in the font. Default value: 0.
    PdfOpNumber max_width;

    /// The width to use for character codes whose widths are not specified in a
    /// font dictionary’s Widths array. This shall have a predictable effect
    /// only if all such codes map to glyphs whose actual widths are the same as
    /// the value of the MissingWidth entry. Default value: 0.
    PdfOpNumber missing_width;

    /// A stream containing a Type 1 font program (see 9.9, "Embedded Font
    /// Programs").
    PdfOpStream font_file;

    /// A stream containing a TrueType font program (see 9.9, "Embedded Font
    /// Programs").
    PdfOpStream font_file2;

    /// A stream containing a font program whose format is specified by the
    /// Subtype entry in the stream dictionary (see Table 126).
    PdfOpStream font_file3;

    /// A string listing the character names defined in a font subset. The names
    /// in this string shall be in PDF syntax—that is, each name preceded by a
    /// slash (/). The names may appear in any order. The name . notdef shall be
    /// omitted; it shall exist in the font subset. If this entry is absent, the
    /// only indication of a font subset shall be the subset tag in the FontName
    /// entry (see 9.6.4, "Font Subsets").
    PdfOpString char_set;

    const PdfObject* raw_dict;
} PdfFontDescriptor;

DESERIALIZABLE_STRUCT_REF(PdfFontDescriptor, font_descriptor)

PdfError* pdf_deserialize_font_descriptor(
    const PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    PdfFontDescriptor* deserialized
);
