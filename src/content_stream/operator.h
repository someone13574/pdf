#pragma once

#include "../ctx.h"
#include "pdf/error.h"

typedef enum {
    // General graphics state
    PDF_OPERATOR_w, // Set the line width in the graphics state.
    PDF_OPERATOR_J, // Set the line cap style in the graphics state.
    PDF_OPERATOR_j, // Set the line join style in the graphics state.
    PDF_OPERATOR_M, // Set the miter limit in the graphics state.
    PDF_OPERATOR_d, // Set the line dash pattern in the graphics state.
    PDF_OPERATOR_ri, // Set the colour rendering intent in the graphics state.
    PDF_OPERATOR_i, // Set the flatness tolerance in the graphics state
                    // (see 10.6.2, "Flatness Tolerance"). flatness is a number
                    // in the range 0 to 100; a value of 0 shall specify the
                    // output device’s default flatness tolerance.
    PDF_OPERATOR_gs, // Set the specified parameters in the graphics state.
                     // dictName shall be the name of a graphics state parameter
                     // dictionary in the ExtGState subdictionary of the current
                     // resource dictionary.

    // Special graphics state
    PDF_OPERATOR_q, // Save the current graphics state on the graphics state
                    // stack.
    PDF_OPERATOR_Q, // Restore the graphics state by removing the most recently
                    // saved state from the stack and making it the current
                    // state.
    PDF_OPERATOR_cm, // Modify the current transformation matrix (CTM) by
                     // concatenating the specified matrix (see 8.3.2,
                     // "Coordinate Spaces"). Although the operands specify a
                     // matrix, they shall be written as six separate numbers,
                     // not as an array.

    // Path construction
    PDF_OPERATOR_m, // Begin a new subpath by moving the current point to
                    // coordinates (x, y), omitting any connecting line segment.
                    // If the previous path construction operator in the current
                    // path was also m, the new m overrides it; no vestige of
                    // the previous m operation remains in the path.
    PDF_OPERATOR_l, // Append a straight line segment from the current point to
                    // the point (x, y). The new current point shall be (x, y).
    PDF_OPERATOR_c, // Append a cubic Bézier curve to the current path. The
                    // curve shall extend from the current point to the point
                    // (x3, y3), using (x1, y1) and (x2, y2) as the Bézier
                    // control points (see 8.5.2.2, "Cubic Bézier Curves"). The
                    // new current point shall be (x3, y3).
    PDF_OPERATOR_v, // Append a cubic Bézier curve to the current path. The
                    // curve shall extend from the current point to the point
                    // (x3, y3), using the current point and (x2, y2) as the
                    // Bézier control points (see 8.5.2.2, "Cubic Bézier
                    // Curves"). The new current point shall be (x3, y3).
    PDF_OPERATOR_y, // Append a cubic Bézier curve to the current path. The
                    // curve shall extend from the current point to the point
                    // (x3, y3), using (x1, y1) and (x3, y3) as the Bézier
                    // control points (see 8.5.2.2, "Cubic Bézier Curves"). The
                    // new current point shall be (x3, y3).
    PDF_OPERATOR_h, // Close the current subpath by appending a straight line
                    // segment from the current point to the starting point of
                    // the subpath. If the current subpath is already closed, h
                    // shall do nothing. This operator terminates the current
                    // subpath. Appending another segment to the current path
                    // shall begin a new subpath, even if the new segment begins
                    // at the endpoint reached by the h operation.
    PDF_OPERATOR_re, // Append a rectangle to the current path as a complete
                     // subpath, with lower-left corner (x, y) and dimensions
                     // width and height in user space. The operation x y width
                     // height re is equivalent to:
                     // ```
                     // x y m
                     // ( x + width) y l
                     // ( x + width) ( y + height) l
                     // x ( y + height) l
                     // h
                     // ```

    // Path painting
    PDF_OPERATOR_S, // Stroke the path.
    PDF_OPERATOR_s, // Close and stroke the path. This operator shall have the
                    // same effect as the sequence h S.
    PDF_OPERATOR_f, // Fill the path, using the nonzero winding number rule to
                    // determine the region to fill (see 8.5.3.3.2, "Nonzero
                    // Winding Number Rule"). Any subpaths that are open shall
                    // be implicitly closed before being filled.
    PDF_OPERATOR_F, // Equivalent to f; included only for compatibility.
                    // Although PDF reader applications shall be able to accept
                    // this operator, PDF writer applications should use f
                    // instead.
    PDF_OPERATOR_f_star, // Fill the path, using the even-odd rule to determine
                         // the region to fill (see 8.5.3.3.3, "Even-Odd Rule").
    PDF_OPERATOR_B, // Fill and then stroke the path, using the nonzero winding
                    // number rule to determine the region to fill. This
                    // operator shall produce the same result as constructing
                    // two identical path objects, painting the first with f and
                    // the second with S.
    PDF_OPERATOR_B_star, // Fill and then stroke the path, using the even-odd
                         // rule to determine the region to fill. This operator
                         // shall produce the same result as B, except that the
                         // path is filled as if with f* instead of f. See
                         // also 11.7.4.4, "Special Path-Painting
                         // Considerations".
    PDF_OPERATOR_b, // Close, fill, and then stroke the path, using the nonzero
                    // winding number rule to determine the region to fill. This
                    // operator shall have the same effect as the sequence h B.
                    // See also 11.7.4.4, "Special Path-Painting
                    // Considerations".
    PDF_OPERATOR_b_star, // lose, fill, and then stroke the path, using the
                         // even-odd rule to determine the region to fill. This
                         // operator shall have the same effect as the sequence
                         // h B*. See also 11.7.4.4, "Special Path-Painting
                         // Considerations".
    PDF_OPERATOR_n, // End the path object without filling or stroking it. This
                    // operator shall be a path- painting no-op, used primarily
                    // for the side effect of changing the current clipping path
                    // (see 8.5.4, "Clipping Path Operators").

    // Clipping paths
    PDF_OPERATOR_W, // Modify the current clipping path by intersecting it with
                    // the current path, using the nonzero winding number rule
                    // to determine which regions lie inside the clipping path.
    PDF_OPERATOR_W_star, // Modify the current clipping path by intersecting it
                         // with the current path, using the even-odd rule to
                         // determine which regions lie inside the clipping
                         // path.

    // Text objects
    PDF_OPERATOR_BT, // Begin a text object, initializing the text matrix, Tm,
                     // and the text line matrix, Tlm, to the identity matrix.
                     // Text objects shall not be nested; a second BT shall not
                     // appear before an ET.
    PDF_OPERATOR_ET, // End a text object, discarding the text matrix.

    // Text state
    PDF_OPERATOR_Tc, // Set the character spacing, Tc, to charSpace, which
                     // shall be a number expressed in unscaled text space
                     // units. Character spacing shall be used by the Tj, TJ,
                     // and ' operators. Initial value: 0.
    PDF_OPERATOR_Tw, // Set the word spacing, Tw, to wordSpace, which shall be a
                     // number expressed in unscaled text space units. Word
                     // spacing shall be used by the Tj, TJ, and ' operators.
                     // Initial value: 0.
    PDF_OPERATOR_Tz, // Set the horizontal scaling, Th, to (scale ÷ 100). scale
                     // shall be a number specifying the percentage of the
                     // normal width. Initial value: 100 (normal width).
    PDF_OPERATOR_TL, // Set the text leading, Tl, to leading, which shall be a
                     // number expressed in unscaled text space units. Text
                     // leading shall be used only by the T*, ', and "
                     // operators. Initial value: 0.
    PDF_OPERATOR_Tf, // Set the text font, Tf, to font and the text font size,
                     // Tfs, to size. font shall be the name of a font resource
                     // in the Font subdictionary of the current resource
                     // dictionary; size shall be a number representing a scale
                     // factor. There is no initial value for either font or
                     // size; they shall be specified explicitly by using Tf
                     // before any text is shown.
    PDF_OPERATOR_Tr, // Set the text rendering mode, Tmode, to render, which
                     // shall be an integer. Initial value: 0.
    PDF_OPERATOR_Ts, // Set the text rise, Trise, to rise, which shall be a
                     // number expressed in unscaled text space units. Initial
                     // value: 0.

    // Text positioning
    PDF_OPERATOR_Td, // Move to the start of the next line, offset from the
                     // start of the current line by (tx, ty). tx and ty shall
                     // denote numbers expressed in unscaled text space units.
    PDF_OPERATOR_TD, // Move to the start of the next line, offset from the
                     // start of the current line by (tx, ty). As a side
                     // effect, this operator shall set the leading parameter in
                     // the text state.
    PDF_OPERATOR_Tm, // Set the text matrix, Tm, and the text line matrix, Tlm
    PDF_OPERATOR_T_star, // Move to the start of the next line. This operator
                         // has the same effect as the code 0 -Tl Td where Tl
                         // denotes the current leading parameter in the text
                         // state. The negative of Tl is used here because Tl is
                         // the text leading expressed as a positive number.
                         // Going to the next line entails decreasing the y
                         // coordinate.

    // Text showing
    PDF_OPERATOR_Tj, // Show a text string.
    PDF_OPERATOR_TJ, // Show one or more text strings, allowing
                     // individual glyph positioning. Each element of
                     // array shall be either a string or a number. If
                     // the element is a string, this operator shall
                     // show the string. If it is a number, the
                     // operator shall adjust the text position by
                     // that amount; that is, it shall translate the
                     // text matrix, Tm. The number shall be
                     // expressed in thousandths of a unit of text
                     // space (see 9.4.4, "Text Space Details"). This
                     // amount shall be subtracted from the current
                     // horizontal or vertical coordinate, depending
                     // on the writing mode. In the default coordinate
                     // system, a positive adjustment has the effect
                     // of moving the next glyph painted either to the
                     // left or down by the given amount. Figure 46
                     // shows an example of the effect of passing
                     // offsets to TJ.
    PDF_OPERATOR_single_quote, //  Move to the next line and show a text string.
                               //  This
                               // operator shall have the same effect as the
                               // code T*
                               // __string__ Tj
    PDF_OPERATOR_double_quote, // Move to the next line and show a text string,
                               // using aw as the word spacing and ac as the
                               // character spacing (setting the corresponding
                               // parameters in the text state). aw and ac shall
                               // be numbers expressed in unscaled text space
                               // units. This operator shall have the same
                               // effect as this code:
                               // aw Tw
                               // ac Tc
                               // __string__ '

    // Type 3 fonts
    PDF_OPERATOR_d0,
    PDF_OPERATOR_d1,

    // Color
    PDF_OPERATOR_CS, // Set the current colour space to use for stroking
                     // operations.
    PDF_OPERATOR_cs, // Same as CS but used for nonstroking operations.
    PDF_OPERATOR_SC, // Set the colour to use for stroking operations in a
                     // device, CIE- based (other than ICCBased), or Indexed
                     // colour space. The number of operands required and their
                     // interpretation depends on the current stroking colour
                     // space.
    PDF_OPERATOR_SCN, // Same as SC but also supports Pattern, Separation,
                      // DeviceN and ICCBased colour spaces.
    PDF_OPERATOR_sc, // Same as SC but used for nonstroking operations.
    PDF_OPERATOR_scn, // Same as SCN but used for nonstroking operations.
    PDF_OPERATOR_G, // Set the stroking colour space to DeviceGray (or the
                    // DefaultGray colour space; see 8.6.5.6, "Default Colour
                    // Spaces") and set the gray level to use for stroking
                    // operations. gray shall be a number between 0.0 (black)
                    // and 1.0 (white).
    PDF_OPERATOR_g, // Same as G but used for nonstroking operations.
    PDF_OPERATOR_RG, // Set the stroking colour space to DeviceRGB (or the
                     // DefaultRGB colour space; see 8.6.5.6, "Default Colour
                     // Spaces") and set the colour to use for stroking
                     // operations. Each operand shall be a number between 0.0
                     // (minimum intensity) and 1.0 (maximum intensity).
    PDF_OPERATOR_rg, // Same as RG but used for nonstroking operations.
    PDF_OPERATOR_K, // Set the stroking colour space to DeviceCMYK (or the
                    // DefaultCMYK colour space; see 8.6.5.6, "Default Colour
                    // Spaces") and set the colour to use for stroking
                    // operations. Each operand shall be a number between 0.0
                    // (zero concentration) and 1.0 (maximum concentration). The
                    // behaviour of this operator is affected by the overprint
                    // mode (see 8.6.7, "Overprint Control").
    PDF_OPERATOR_k, // Same as K but used for nonstroking operations.

    // Shading patterns
    PDF_OPERATOR_sh, // Paint the shape and colour shading described by a
                     // shading dictionary, subject to the current clipping
                     // path. The current colour in the graphics state is
                     // neither used nor altered. The effect is different from
                     // that of painting a path using a shading pattern as the
                     // current colour.

    // Inline Images
    PDF_OPERATOR_BI, // Begin an inline image object.
    PDF_OPERATOR_ID, // Begin the image data for an inline image object.
    PDF_OPERATOR_EI, // End an inline image object.

    // XObjects
    PDF_OPERATOR_Do, // Paint the specified XObject. The operand name shall
                     // appear as a key in the XObject subdictionary of the
                     // current resource dictionary (see 7.8.3, "Resource
                     // Dictionaries"). The associated value shall be a stream
                     // whose Type entry, if present, is XObject. The effect of
                     // Do depends on the value of the XObject’s Subtype entry,
                     // which may be Image (see 8.9.5, "Image Dictionaries"),
                     // Form (see 8.10, "Form XObjects"), or PS (see 8.8.2,
                     // "PostScript XObjects").

    // Marked content
    PDF_OPERATOR_MP, // Designate a marked-content point. tag shall be a name
                     // object indicating the role or significance of the point.
    PDF_OPERATOR_DP, // Designate a marked-content point with an associated
                     // property list. tag shall be a name object indicating the
                     // role or significance of the point. properties shall be
                     // either an inline dictionary containing the property list
                     // or a name object associated with it in the Properties
                     // subdictionary of the current resource dictionary
                     // (see 14.6.2, “Property Lists”).
    PDF_OPERATOR_BMC, // Begin a marked-content sequence terminated by a
                      // balancing EMC operator. tag shall be a name object
                      // indicating the role or significance of the sequence.
    PDF_OPERATOR_BDC, // Begin a marked-content sequence with an associated
                      // property list, terminated by a balancing EMC operator.
                      // tag shall be a name object indicating the role or
                      // significance of the sequence. properties shall be
                      // either an inline dictionary containing the property
                      // list or a name object associated with it in the
                      // Properties subdictionary of the current resource
                      // dictionary (see 14.6.2, “Property Lists”).
    PDF_OPERATOR_EMC, // End a marked-content sequence begun by a BMC or BDC
                      // operator.

    // Compatibility
    PDF_OPERATOR_BX, // Begin a compatibility section. Unrecognized operators
                     // (along with their operands) shall be ignored without
                     // error until the balancing EX operator is encountered.
    PDF_OPERATOR_EX // End a compatibility section begun by a balancing BX
                    // operator. Ignore any unrecognized operands and operators
                    // from previous matching BX onward.
} PdfOperator;

PdfError* pdf_parse_operator(PdfCtx* ctx, PdfOperator* operator);
