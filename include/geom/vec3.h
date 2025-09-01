#pragma once

/// A three element vector
#include "geom/mat3.h"
typedef struct {
    double x;
    double y;
    double z;
} GeomVec3;

/// Creates a new vector
GeomVec3 geom_vec3_new(double x, double y, double z);

/// Transforms this vector using a 3x3 matrix
GeomVec3 geom_vec3_transform(GeomVec3 lhs, GeomMat3 rhs);
