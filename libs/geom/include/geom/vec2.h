#pragma once

#include <stdbool.h>

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

/// Computes the difference of vectors a and b.
GeomVec2 geom_vec2_sub(GeomVec2 a, GeomVec2 b);

/// Scales a vector by a scalar.
GeomVec2 geom_vec2_scale(GeomVec2 vec, double scalar);

/// Computes the dot product of vectors a and b.
double geom_vec2_dot(GeomVec2 a, GeomVec2 b);

/// Computes the 2D cross product of vectors a and b.
double geom_vec2_cross(GeomVec2 a, GeomVec2 b);

/// Computes the squared length of a vector.
double geom_vec2_len_sq(GeomVec2 vec);

/// Computes a normalized vector. Returns zero vector when input is ~zero.
GeomVec2 geom_vec2_normalize(GeomVec2 vec);

/// Computes the angle from `from` to `to` in radians.
double geom_vec2_angle(GeomVec2 from, GeomVec2 to);

/// Computes the linear interpolation of vectors a and b.
GeomVec2 geom_vec2_lerp(GeomVec2 a, GeomVec2 b, double t);

/// Gets a point made up of the minimum of each axis
GeomVec2 geom_vec2_min(GeomVec2 a, GeomVec2 b);

/// Gets a point made up of the maximum of each axis
GeomVec2 geom_vec2_max(GeomVec2 a, GeomVec2 b);

/// Returns true when both component deltas are <= epsilon
bool geom_vec2_equal_eps(GeomVec2 a, GeomVec2 b, double epsilon);

/// Transforms this vector using a 3x3 matrix
GeomVec2 geom_vec2_transform(GeomVec2 lhs, GeomMat3 rhs);

/// Swaps the x and y components of this vector
GeomVec2 geom_vec2_transpose(GeomVec2 vec);

/// Rotates a vector 90 degrees counter-clockwise.
GeomVec2 geom_vec2_perpendicular(GeomVec2 vec);
