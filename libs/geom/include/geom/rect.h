#pragma once

#include <stdbool.h>

#include "geom/vec2.h"

/// A rectangle
typedef struct {
    GeomVec2 min;
    GeomVec2 max;
} GeomRect;

/// Creates a new rectangle from two points
GeomRect geom_rect_new(GeomVec2 point_a, GeomVec2 point_b);

/// Creates a new rectangle from a point and a radius
GeomRect geom_rect_new_centered(GeomVec2 center, GeomVec2 radius);

/// Floors the min and ceils the max
GeomRect geom_rect_round(GeomRect rect);

/// Gets the size of the rectangle
GeomVec2 geom_rect_size(GeomRect rect);

/// Gets the smallest rectangle large enough to hold both rectangles
GeomRect geom_rect_union(GeomRect a, GeomRect b);

/// Gets the rectangle defined by the intersection of a and b. If there is no
/// intersection, the rectangle will have zero or negative size.
GeomRect geom_rect_intersection(GeomRect a, GeomRect b);

/// Calculates the area of the rectangle. This can be negative.
double geom_rect_area(GeomRect rect);

/// Returns true if the area is negative or zero
bool geom_rect_positive(GeomRect rect);

/// Finds the bounding rectangle of a transformed rectangle
GeomRect geom_rect_transform(GeomRect rect, GeomMat3 transform);
