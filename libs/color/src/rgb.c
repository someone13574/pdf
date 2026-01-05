#include "color/rgb.h"

Rgb rgba(double r, double g, double b) {
    return (Rgb) {.r = r, .g = g, .b = b};
}

Rgba rgba_new(double r, double g, double b, double a) {
    return (Rgba) {.r = r, .g = g, .b = b, .a = a};
}

Rgba rgba_from_rgb(Rgb rgb, double alpha) {
    return (Rgba) {.r = rgb.r, .g = rgb.g, .b = rgb.b, .a = alpha};
}

uint32_t rgba_pack(Rgba rgba) {
    return ((uint32_t)(rgba.r * 255.0) << 24)
         | ((uint32_t)(rgba.g * 255.0) << 16)
         | ((uint32_t)(rgba.b * 255.0) << 8) | (uint32_t)(rgba.a * 255.0);
}

Rgba rgba_unpack(uint32_t packed_rgba) {
    return rgba_new(
        (double)((packed_rgba >> 24) & 0xff) / 255.0,
        (double)((packed_rgba >> 16) & 0xff) / 255.0,
        (double)((packed_rgba >> 8) & 0xff) / 255.0,
        (double)(packed_rgba & 0xff) / 255.0
    );
}
