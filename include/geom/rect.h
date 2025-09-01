#pragma once

/// A rectangle
#include "geom/vec2.h"
typedef struct {
    GeomVec2 min;
    GeomVec2 max;
} GeomRect;

/// Creates a new rectangle from two points
GeomRect geom_rect_new(GeomVec2 point_a, GeomVec2 point_b);

/// Gets the size of the rectangle
GeomVec2 geom_rect_size(GeomRect rect);
