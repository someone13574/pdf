#pragma once

#include "err/error.h"
#include "pdf/color_space.h"
#include "pdf/function.h"
#include "pdf/object.h"
#include "pdf/resolver.h"

/// Axial shadings
typedef struct {
    /// (Required) An array of four numbers [ x0 y0 x1 y1 ] specifying the
    /// starting and ending coordinates of the axis, expressed in the shading’s
    /// target coordinate space.
    PdfNumberVec* coords;

    /// (Optional) An array of two numbers [ t0 t1 ] specifying the limiting
    /// values of a parametric variable t. The variable is considered to vary
    /// linearly between these two values as the colour gradient varies between
    /// the starting and ending points of the axis. The variable t becomes the
    /// input argument to the colour function(s). Default value: [ 0.0 1.0 ].
    PdfNumberVecOptional domain;

    /// (Required) A 1-in, n-out function or an array of n 1-in, 1-out functions
    /// (where n is the number of colour components in the shading dictionary’s
    /// colour space). The function(s) shall be called with values of the
    /// parametric variable t in the domain defined by the Domain entry. Each
    /// function’s domain shall be a superset of that of the shading dictionary.
    /// If the value returned by the function for a given colour component is
    /// out of range, it shall be adjusted to the nearest valid value.
    PdfFunctionVec* function;

    /// (Optional) An array of two boolean values specifying whether to extend
    /// the shading beyond the starting and ending points of the axis,
    /// respectively. Default value: [ false false ].
    PdfBooleanVecOptional extend;
} PdfShadingDictType2;

/// Radial shadings
typedef struct {
    /// (Required) An array of six numbers [ x0 y0 r0 x1 y1 r1 ] specifying the
    /// centres and radii of the starting and ending circles, expressed in the
    /// shading’s target coordinate space. The radii r0 and r1 shall both be
    /// greater than or equal to 0. If one radius is 0, the corresponding circle
    /// shall be treated as a point; if both are 0, nothing shall be painted.
    PdfNumberVec* coords;

    /// (Optional) An array of two numbers [ t0 t1 ] specifying the limiting
    /// values of a parametric variable t. The variable is considered to vary
    /// linearly between these two values as the colour gradient varies between
    /// the starting and ending circles. The variable t becomes the input
    /// argument to the colour function(s). Default value: [ 0.0 1.0 ].
    PdfNumberVecOptional domain;

    /// (Required) A 1-in, n-out function or an array of n 1-in, 1-out functions
    /// (where n is the number of colour components in the shading dictionary’s
    /// colour space). The function(s) shall be called with values of the
    /// parametric variable t in the domain defined by the shading dictionary’s
    /// Domain entry. Each function’s domain shall be a superset of that of the
    /// shading dictionary. If the value returned by the function for a given
    /// colour component is out of range, it shall be adjusted to the nearest
    /// valid value.
    PdfFunctionVec* function;

    /// (Optional) An array of two boolean values specifying whether to extend
    /// the shading beyond the starting and ending circles, respectively.
    /// Default value: [ false false ].
    PdfBooleanVecOptional extend;
} PdfShadingDictType3;

typedef union {
    PdfShadingDictType2 type2;
    PdfShadingDictType3 type3;
} PdfShadingDictData;

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

    /// Type specific data
    PdfShadingDictData data;
} PdfShadingDict;

Error* pdf_deserialize_shading_dict(
    const PdfObject* object,
    PdfShadingDict* target_ptr,
    PdfResolver* resolver
);
