#pragma once

#include "err/error.h"
#include "pdf/fonts/cid_to_gid_map.h"
#include "pdf/fonts/cmap.h"
#include "pdf/fonts/encoding.h"
#include "pdf/fonts/font_descriptor.h"
#include "pdf/fonts/font_widths.h"
#include "pdf/object.h"
#include "pdf/resolver.h"

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

    /// (Optional; applies only to CIDFonts used for vertical writing) An array
    /// of two numbers specifying the default metrics for vertical writing
    /// (see 9.7.4.3, "Glyph Metrics in CIDFonts"). Default value: [ 880 −1000
    /// ].
    PdfUnimplemented dw2;

    /// (Optional; applies only to CIDFonts used for vertical writing) A
    /// description of the metrics for vertical writing for the glyphs in the
    /// CIDFont (see 9.7.4.3, "Glyph Metrics in CIDFonts"). Default value: none
    /// (the DW2 value shall be used for all glyphs).
    PdfUnimplemented w2;

    /// (Optional; Type 2 CIDFonts only) A specification of the mapping from
    /// CIDs to glyph indices. If the value is a stream, the bytes in the stream
    /// shall contain the mapping from CIDs to glyph indices: the glyph index
    /// for a particular CID value c shall be a 2-byte value stored in bytes 2 ×
    /// c and 2 × c + 1, where the first byte shall be the high-order byte. If
    /// the value of CIDToGIDMap is a name, it shall be Identity, indicating
    /// that the mapping between CIDs and glyph indices is the identity mapping.
    /// Default value: Identity.
    PdfCIDToGIDMapOptional cid_to_gid_map;
} PdfCIDFont;

Error* pdf_deserialize_cid_font(
    const PdfObject* object,
    PdfCIDFont* target_ptr,
    PdfResolver* resolver
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
} PdfType0font;

Error* pdf_deserialize_type0_font(
    const PdfObject* object,
    PdfType0font* target_ptr,
    PdfResolver* resolver
);

typedef struct {
    /// Required) The type of PDF object that this dictionary describes; shall
    /// be Font for a font dictionary.
    PdfName type;

    /// (Required) The type of font; shall be TrueType.
    PdfName subtype;

    /// (Required) The PostScript name for the value of BaseFont may be
    /// determined in one of two ways: If the TrueType font program's “name”
    /// table contains a PostScript name, it shall be used. In the absence of
    /// such an entry in the “name” table, a PostScript name shall be derived
    /// from the name by which the font is known in the host operating system.On
    /// a Windows system, the name shall be based on the lfFaceName field in a
    /// LOGFONT structure; in the Mac OS, it shall be based on the name of the
    /// FOND resource.If the name contains any SPACEs, the SPACEs shall be
    /// removed.
    PdfName base_font;

    /// (Required except for the standard 14 fonts) The first character code
    /// defined in the font’s Widths array. Beginning with PDF 1.5, the special
    /// treatment given to the standard 14 fonts is deprecated. Conforming
    /// writers should represent all fonts using a complete font descriptor. For
    /// backwards capability, conforming readers shall still provide the special
    /// treatment identified for the standard 14 fonts.
    PdfIntegerOptional first_char;

    /// (Required except for the standard 14 fonts) The last character code
    /// defined in the font’s Widths array. Beginning with PDF 1.5, the special
    /// treatment given to the standard 14 fonts is deprecated. Conforming
    /// writers should represent all fonts using a complete font descriptor. For
    /// backwards capability, conforming readers shall still provide the special
    /// treatment identified for the standard 14 fonts.
    PdfIntegerOptional last_char;

    /// (Required except for the standard 14 fonts; indirect reference
    /// preferred) An array of (LastChar − FirstChar + 1) widths, each element
    /// being the glyph width for the character code that equals FirstChar plus
    /// the array index. For character codes outside the range FirstChar to
    /// LastChar, the value of MissingWidth from the FontDescriptor entry for
    /// this font shall be used. The glyph widths shall be measured in units in
    /// which 1000 units correspond to 1 unit in text space. These widths shall
    /// be consistent with the actual widths given in the font program. For more
    /// information on glyph widths and other glyph metrics, see 9.2.4, "Glyph
    /// Positioning and Metrics".
    PdfNumberVecOptional widths;

    /// (Required except for the standard 14 fonts; shall be an indirect
    /// reference) A font descriptor describing the font’s metrics other than
    /// its glyph widths (see 9.8, "Font Descriptors"”\). For the standard 14
    /// fonts, the entries FirstChar, LastChar, Widths, and FontDescriptor shall
    /// either all be present or all be absent. Ordinarily, these dictionary
    /// keys may be absent; specifying them enables a standard font to be
    /// overridden; see 9.6.2.2, "Standard Type 1 Fonts (Standard 14 Fonts)".
    PdfFontDescriptorRefOptional font_descriptor;

    /// (Optional) A specification of the font’s character encoding if different
    /// from its built-in encoding. The value of Encoding shall be either the
    /// name of a predefined encoding (MacRomanEncoding, MacExpertEncoding, or
    /// WinAnsiEncoding, as described in Annex D) or an encoding dictionary that
    /// shall specify differences from the font’s built-in encoding or from a
    /// specified predefined encoding (see 9.6.6, "Character Encoding").
    PdfEncodingDictOptional encoding;

    /// (Optional; PDF 1.2) A stream containing a CMap file that maps character
    /// codes to Unicode values (see 9.10, "Extraction of Text Content").
    PdfStreamOptional to_unicode;
} PdfTrueTypeFont;

Error* pdf_deserialize_truetype_font_dict(
    const PdfObject* object,
    PdfTrueTypeFont* target_ptr,
    PdfResolver* resolver
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
        PdfTrueTypeFont true_type;
    } data;
} PdfFont;

Error* pdf_deserialize_font(
    const PdfObject* object,
    PdfFont* deserialized,
    PdfResolver* resolver
);
