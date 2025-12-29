#pragma once

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
    PdfResolver* resolver
);

DESERDE_DECL_OPTIONAL(PdfResourcesOptional, PdfResources, pdf_resources_op_init)
DESERDE_DECL_TRAMPOLINE(pdf_deserialize_resources_trampoline)

typedef enum {
    PDF_LINE_CAP_STYLE_BUTT,
    PDF_LINE_CAP_STYLE_ROUND,
    PDF_LINE_CAP_STYLE_PROJECTING
} PdfLineCapStyle;

typedef enum {
    PDF_LINE_JOIN_STYLE_MITER,
    PDF_LINE_JOIN_STYLE_ROUND,
    PDF_LINE_JOIN_STYLE_BEVEL
} PdfLineJoinStyle;

typedef struct {
    /// (Optional) The type of PDF object that this dictionary describes; shall
    /// be ExtGState for a graphics state parameter dictionary.
    PdfNameOptional type;

    /// (Optional; PDF 1.3) The line width (see 8.4.3.2, "Line Width").
    PdfUnimplemented line_width;

    /// (Optional; PDF 1.3) The line cap style (see 8.4.3.3, "Line Cap Style").
    PdfUnimplemented line_cap;

    /// (Optional; PDF 1.3) The line join style (see 8.4.3.4, "Line Join
    /// Style").
    PdfUnimplemented line_join;

    /// (Optional; PDF 1.3) The miter limit (see 8.4.3.5, "Miter Limit").
    PdfUnimplemented miter_limit;

    /// (Optional; PDF 1.3) The line dash pattern, expressed as an array of the
    /// form [ dashArray dashPhase ], where dashArray shall be itself an array
    /// and dashPhase shall be an integer (see 8.4.3.6, "Line Dash Pattern").
    PdfUnimplemented dash_pattern;

    /// (Optional; PDF 1.3) The name of the rendering intent
    /// (see 8.6.5.8,"Rendering Intents").
    PdfUnimplemented rendering_intent;

    /// (Optional) A flag specifying whether to apply overprint (see 8.6.7,
    /// "Overprint Control"). In PDF 1.2 and earlier, there is a single
    /// overprint parameter that applies to all painting operations. Beginning
    /// with PDF 1.3, there shall be two separate overprint parameters: one for
    /// stroking and one for all other painting operations. Specifying an OP
    /// entry shall set both parameters unless there is also an op entry in the
    /// same graphics state parameter dictionary, in which case the OP entry
    /// shall set only the overprint parameter for stroking.
    PdfUnimplemented overprint_upper;

    /// (Optional; PDF 1.3) A flag specifying whether to apply overprint
    /// (see 8.6.7, "Overprint Control") for painting operations other than
    /// stroking. If this entry is absent, the OP entry, if any, shall also set
    /// this parameter.
    PdfUnimplemented overprint_lower;

    /// (Optional; PDF 1.3) The overprint mode (see 8.6.7, "Overprint Control").
    PdfUnimplemented overprint_mode;

    /// (Optional; PDF 1.3) An array of the form [ font size ], where font shall
    /// be an indirect reference to a font dictionary and size shall be a number
    /// expressed in text space units. These two objects correspond to the
    /// operands of the Tf operator (see 9.3, "Text State Parameters and
    /// Operators"); however, the first operand shall be an indirect object
    /// reference instead of a resource name.
    PdfUnimplemented font;

    /// (Optional) The black-generation function, which maps the interval [
    /// 0.0 1.0 ] to the interval [ 0.0 1.0 ] (see 10.3.4, "Conversion from
    /// DeviceRGB to DeviceCMYK").
    PdfUnimplemented bg;

    /// (Optional; PDF 1.3) Same as BG except that the value may also be the
    /// name Default, denoting the black-generation function that was in effect
    /// at the start of the page. If both BG and BG2 are present in the same
    /// graphics state parameter dictionary, BG2 shall take precedence.
    PdfUnimplemented bg2;

    /// (Optional) The undercolor-removal function, which maps the interval [
    /// 0.0 1.0 ] to the interval [ −1.0 1.0 ] (see 10.3.4, "Conversion from
    /// DeviceRGB to DeviceCMYK").
    PdfUnimplemented ucr;

    /// (Optional; PDF 1.3) Same as UCR except that the value may also be the
    /// name Default, denoting the undercolor-removal function that was in
    /// effect at the start of the page. If both UCR and UCR2 are present in the
    /// same graphics state parameter dictionary, UCR2 shall take precedence.
    PdfUnimplemented ucr2;

    /// (Optional) The transfer function, which maps the interval [ 0.0 1.0 ] to
    /// the interval [ 0.0 1.0 ] (see 10.4, "Transfer Functions"). The value
    /// shall be either a single function (which applies to all process
    /// colorants) or an array of four functions (which apply to the process
    /// colorants individually). The name Identity may be used to represent the
    /// identity function.
    PdfUnimplemented tr;

    /// (Optional; PDF 1.3) Same as TR except that the value may also be the
    /// name Default, denoting the transfer function that was in effect at the
    /// start of the page. If both TR and TR2 are present in the same graphics
    /// state parameter dictionary, TR2 shall take precedence.
    PdfUnimplemented tr2;

    /// (Optional) The halftone dictionary or stream (see 10.5, "Halftones") or
    /// the name Default, denoting the halftone that was in effect at the start
    /// of the page.
    PdfUnimplemented ht;

    /// (Optional; PDF 1.3) The flatness tolerance (see 10.6.2, "Flatness
    /// Tolerance").
    PdfUnimplemented fl;

    /// (Optional; PDF 1.3) The smoothness tolerance (see 10.6.3, "Smoothness
    /// Tolerance").
    PdfRealOptional sm;

    /// (Optional) A flag specifying whether to apply automatic stroke
    /// adjustment (see 10.6.5, "Automatic Stroke Adjustment").
    PdfBooleanOptional sa;

    /// (Optional; PDF 1.4) The current blend mode to be used in the transparent
    /// imaging model (see 11.3.5, "Blend Mode" and 11.6.3, "Specifying Blending
    /// Colour Space and Blend Mode").
    PdfUnimplemented bm;

    /// (Optional; PDF 1.4) The current soft mask, specifying the mask shape or
    /// mask opacity values that shall be used in the transparent imaging model
    /// (see 11.3.7.2, "Source Shape and Opacity" and 11.6.4.3, "Mask Shape and
    /// Opacity"). Although the current soft mask is sometimes referred to as a
    /// “soft clip,” altering it with the gs operator completely replaces the
    /// old value with the new one, rather than intersecting the two as is done
    /// with the current clipping path parameter (see 8.5.4, "Clipping Path
    /// Operators").
    PdfUnimplemented smask;

    /// (Optional; PDF 1.4) The current stroking alpha constant, specifying the
    /// constant shape or constant opacity value that shall be used for stroking
    /// operations in the transparent imaging model (see 11.3.7.2, "Source Shape
    /// and Opacity" and 11.6.4.4, "Constant Shape and Opacity").
    PdfRealOptional ca_stroking;

    /// (Optional; PDF 1.4) Same as CA, but for nonstroking operations.
    PdfRealOptional ca_nonstroking;

    /// (Optional; PDF 1.4) The alpha source flag (“alpha is shape”), specifying
    /// whether the current soft mask and alpha constant shall be interpreted as
    /// shape values (true) or opacity values (false).
    PdfUnimplemented ais;

    /// (Optional; PDF 1.4) The text knockout flag, shall determine the
    /// behaviour of overlapping glyphs within a text object in the transparent
    /// imaging model (see 9.3.8, "Text Knockout").
    PdfUnimplemented tk;
} PdfGStateParams;

PdfError* pdf_deserialize_gstate_params(
    const PdfObject* object,
    PdfGStateParams* target_ptr,
    PdfResolver* resolver
);
