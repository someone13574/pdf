#pragma once

#include "geom/mat3.h"

/// A three element vector
typedef struct {
    double x;
    double y;
    double z;
} GeomVec3;

/// Creates a new vector
GeomVec3 geom_vec3_new(double x, double y, double z);

/// Creates a new vector from a single value
GeomVec3 geom_vec3_scalar(double v);

/// Computes the sum of vectors a and b.
GeomVec3 geom_vec3_add(GeomVec3 lhs, GeomVec3 rhs);

/// Computes the difference of vectors a and b.
GeomVec3 geom_vec3_sub(GeomVec3 lhs, GeomVec3 rhs);

/// Computes the product of vectors a and b.
GeomVec3 geom_vec3_mul(GeomVec3 lhs, GeomVec3 rhs);

/// Computes the component-wise lhs^rhs
GeomVec3 geom_vec3_pow(GeomVec3 lhs, GeomVec3 rhs);

/// Transforms this vector using a 3x3 matrix
GeomVec3 geom_vec3_transform(GeomVec3 lhs, GeomMat3 rhs);
