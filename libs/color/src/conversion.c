#include "color/conversion.h"

#include "color/cie.h"
#include "color/rgb.h"
#include "geom/mat3.h"
#include "geom/vec3.h"

/// Converts CIE xyz color to non-linear sRGB:
/// https://www.w3.org/Graphics/Color/sRGB.html, https://www.color.org/srgb.pdf
Rgb cie_xyz_to_linear_srgb(CieXYZ cie_xyz) {
    GeomVec3 xyz_vec = cie_xyz_to_geom(cie_xyz);
    GeomVec3 rgb_vec = geom_vec3_transform(
        xyz_vec,
        geom_mat3_new(
            3.2406255,
            -1.537208,
            -0.4986286,
            -0.9689307,
            1.8757561,
            0.0415175,
            0.0557101,
            -0.2040211,
            1.0569959
        )
    );

    return rgb_from_geom(rgb_vec);
}

Rgb cie_xyz_to_srgb(CieXYZ cie_xyz, Rgb whitepoint, Rgb blackpoint) {
    Rgb linear = cie_xyz_to_linear_srgb(cie_xyz);
    return srgb_to_non_linear(linear, whitepoint, blackpoint);
}
