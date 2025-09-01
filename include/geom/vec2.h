#pragma once

/// A two element vector
typedef struct {
    double x;
    double y;
} GeomVec2;

/// Creates a new vector
GeomVec2 geom_vec2_new(double x, double y);
