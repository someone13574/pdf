#pragma once

#include "geom/vec3.h"

typedef struct {
    double x;
    double y;
    double z;
} CieXYZ;

CieXYZ cie_xyz_new(double x, double y, double z);

CieXYZ cie_xyz_from_geom(GeomVec3 vec);
GeomVec3 cie_xyz_to_geom(CieXYZ cie_xyz);
