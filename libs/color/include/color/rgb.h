#pragma once

#include <stdint.h>

#include "geom/vec3.h"

typedef struct {
    double r;
    double g;
    double b;
} Rgb;

typedef struct {
    double r;
    double g;
    double b;
    double a;
} Rgba;

Rgb rgb_new(double r, double g, double b);
Rgba rgba_new(double r, double g, double b, double a);

Rgb rgb_from_rgba(Rgba rgba);
Rgba rgba_from_rgb(Rgb rgb, double alpha);

Rgb rgb_from_geom(GeomVec3 vec);
GeomVec3 rgb_to_geom(Rgb rgb);

uint32_t rgba_pack(Rgba rgba);
Rgba rgba_unpack(uint32_t packed_rgba);

Rgba rgba_blend_src_over(Rgba dst, Rgba src);
Rgb srgb_to_non_linear(Rgb linear_srgb, Rgb whitepoint, Rgb blackpoint);
