#pragma once

#include "pdf/color_space.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"

// typedef union {
//
// } PdfShadingDictData;

typedef struct {
    /// (Required) The shading type:
    /// 1 - Function-based shading
    /// 2 - Axial shading
    /// 3 - Radial shading
    /// 4 - Free-form Gouraud-shaded triangle mesh
    /// 5 - Lattice-form Gouraud-shaded triangle mesh
    /// 6 - Coons patch mesh
    /// 7 - Tensor-product patch mesh
    PdfInteger shading_type;

    /// (Required) The colour space in which colour values shall be expressed.
    /// This may be any device, CIE-based, or special colour space except a
    /// Pattern space. See 8.7.4.4, "Colour Space: Special Considerations" for
    /// further information.
    PdfColorSpace color_space;

    /// (Optional) An array of colour components appropriate to the colour
    /// space, specifying a single background colour value. If present, this
    /// colour shall be used, before any painting operation involving the
    /// shading, to fill those portions of the area to be painted that lie
    /// outside the bounds of the shading object.
    PdfUnimplemented background;

    /// (Optional) An array of four numbers giving the left, bottom, right, and
    /// top coordinates, respectively, of the shading’s bounding box. The
    /// coordinates shall be interpreted in the shading’s target coordinate
    /// space. If present, this bounding box shall be applied as a temporary
    /// clipping boundary when the shading is painted, in addition to the
    /// current clipping path and any other clipping boundaries in effect at
    /// that time.
    PdfRectangleOptional bbox;

    /// (Optional) A flag indicating whether to filter the shading function to
    /// prevent aliasing artifacts.
    PdfBooleanOptional anti_alias;
} PdfShadingDict;

PdfError* pdf_deserialize_shading_dict(
    const PdfObject* object,
    PdfShadingDict* target_ptr,
    PdfResolver* resolver
);
