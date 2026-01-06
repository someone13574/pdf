#pragma once

#include "geom/vec3.h"

typedef struct {
    double x;
    double y;
    double z;
} CieXYZ;

typedef struct {
    double l;
    double a;
    double b;
} CieLab;

CieXYZ cie_xyz_new(double x, double y, double z);
CieLab cie_lab_new(double l, double a, double b);

CieXYZ cie_xyz_from_geom(GeomVec3 vec);
GeomVec3 cie_xyz_to_geom(CieXYZ cie_xyz);

CieLab cie_lab_from_geom(GeomVec3 vec);
GeomVec3 cie_lab_to_geom(CieLab cie_lab);

CieLab cie_xyz_to_cie_lab(CieXYZ cie_xyz, CieXYZ reference_illuminant);
CieXYZ cie_lab_to_cie_xyz(CieLab cie_lab, CieXYZ reference_illuminant);
