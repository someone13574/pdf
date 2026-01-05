#pragma once

#include "color/cie_xyz.h"
#include "color/rgb.h"

Rgb cie_xyz_to_linear_srgb(CieXYZ cie_xyz);
Rgb cie_xyz_to_srgb(CieXYZ cie_xyz, Rgb whitepoint, Rgb blackpoint);
