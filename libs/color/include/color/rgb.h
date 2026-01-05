#pragma once

#include <stdint.h>

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

Rgb rgba(double r, double g, double b);
Rgba rgba_new(double r, double g, double b, double a);
Rgba rgba_from_rgb(Rgb rgb, double alpha);
uint32_t rgba_pack(Rgba rgba);
Rgba rgba_unpack(uint32_t packed_rgba);
