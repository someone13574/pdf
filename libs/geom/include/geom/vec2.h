#pragma once

#include "geom/mat3.h"

/// A two element vector
typedef struct {
    double x;
    double y;
} GeomVec2;

/// Creates a new vector
GeomVec2 geom_vec2_new(double x, double y);

/// Computes the sum of vectors a and b.
GeomVec2 geom_vec2_add(GeomVec2 a, GeomVec2 b);

/// Transforms this vector using a 3x3 matrix
GeomVec2 geom_vec2_transform(GeomVec2 lhs, GeomMat3 rhs);

/// Swaps the x and y components of this vector
GeomVec2 geom_vec2_transpose(GeomVec2 vec);
