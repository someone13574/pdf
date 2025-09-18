#pragma once

#include "arena/arena.h"
#include "pdf/fonts/cmap.h"
#include "pdf/fonts/font_descriptor.h"
#include "pdf/fonts/font_widths.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"

/// TODO: DW2, W2, CIDToGIDMap
typedef struct {
    /// The type of PDF object that this dictionary describes; shall be Font for
    /// a CIDFont dictionary.
    PdfName type;

    /// The type of CIDFont shall be CIDFontType0 or CIDFontType2.
    PdfName subtype;

    /// The PostScript name of the CIDFont. For Type 0 CIDFonts, this shall be
    /// the value of the CIDFontName entry in the CIDFont program. For Type 2
    /// CIDFonts, it shall be derived the same way as for a simple TrueType
    /// font; see 9.6.3, "TrueType Fonts". In either case, the name may have a
    /// subset prefix if appropriate;
    PdfName base_font;

    /// A dictionary containing entries that define the character collection of
    /// the CIDFont.
    PdfCIDSystemInfo cid_system_info;

    /// A font descriptor describing the CIDFont’s default metrics other than
    /// its glyph widths.
    PdfFontDescriptorRef font_descriptor;

    /// The default width for glyphs in the CIDFont. Default value: 1000
    /// (defined in user units).
    PdfIntegerOptional dw;

    // A description of the widths for the glyphs in the CIDFont.
    PdfFontWidthsOptional w;

    const PdfObject* raw_dict;
} PdfCIDFont;

PdfError* pdf_deserialize_cid_font(
    const PdfObject* object,
    PdfCIDFont* target_ptr,
    PdfOptionalResolver resolver,
    Arena* arena
);

DESERDE_DECL_TRAMPOLINE(pdf_deserialize_cid_font_trampoline)

#define DVEC_NAME PdfCIDFontVec
#define DVEC_LOWERCASE_NAME pdf_cid_font_vec
#define DVEC_TYPE PdfCIDFont
#include "arena/dvec_decl.h"

typedef struct {
    /// The type of PDF object that this dictionary describes; shall be Font for
    /// a font dictionary.
    PdfName type;

    /// The type of font; shall be Type0 for a Type 0 font.
    PdfName subtype;

    /// The name of the font. If the descendant is a Type 0 CIDFont, this name
    /// should be the concatenation of the CIDFont’s BaseFont name, a hyphen,
    /// and the CMap name given in the Encoding entry (or the CMapName entry in
    /// the CMap). If the descendant is a Type 2 CIDFont, this name should be
    /// the same as the CIDFont’s BaseFont name.
    PdfName base_font;

    /// The name of a predefined CMap, or a stream containing a CMap that maps
    /// character codes to font numbers and CIDs. If the descendant is a Type 2
    /// CIDFont whose associated TrueType font program is not embedded in the
    /// PDF file, the Encoding entry shall be a predefined CMap name.
    ///
    /// TODO: Support streams
    PdfName encoding;

    /// A one-element array specifying the CIDFont dictionary that is the
    /// descendant of this Type 0 font.
    PdfCIDFontVec* descendant_fonts;

    /// A stream containing a CMap file that maps character codes to Unicode
    /// values.
    PdfStreamOptional to_unicode;

    const PdfObject* raw_dict;
} PdfType0font;

PdfError* pdf_deserialize_type0_font(
    const PdfObject* object,
    PdfType0font* target_ptr,
    PdfOptionalResolver resolver,
    Arena* arena
);

typedef enum {
    /// A composite font—a font composed of glyphs from a
    /// descendant CIDFont
    PDF_FONT_TYPE0,

    /// A font that defines glyph shapes using Type 1 font
    /// technology
    PDF_FONT_TYPE1,

    /// A multiple master font—an extension of the Type 1 font
    /// that allows the generation of a wide variety of
    /// typeface styles from a single font
    PDF_FONT_MMTYPE1,

    /// A font that defines glyphs with streams of PDF graphics
    /// operators
    PDF_FONT_TYPE3,

    /// A font based on the TrueType font format
    PDF_FONT_TRUETYPE,

    /// A CIDFont whose glyph descriptions are based on Type
    /// 1 font technology
    PDF_FONT_CIDTYPE0,

    /// A CIDFont whose glyph descriptions are based on
    /// TrueType font technology
    PDF_FONT_CIDTYPE2
} PdfFontSubtype;

typedef struct {
    PdfFontSubtype type;
    union {
        PdfType0font type0;
        PdfCIDFont cid;
    } data;
} PdfFont;

PdfError* pdf_deserialize_font(
    const PdfObject* object,
    PdfFont* deserialized,
    PdfOptionalResolver resolver,
    Arena* arena
);
