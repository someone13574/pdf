#include "color/rgb.h"

#include <math.h>

#include "geom/vec3.h"

Rgb rgb_new(double r, double g, double b) {
    return (Rgb) {.r = r, .g = g, .b = b};
}

Rgba rgba_new(double r, double g, double b, double a) {
    return (Rgba) {.r = r, .g = g, .b = b, .a = a};
}

Rgb rgb_from_rgba(Rgba rgba) {
    return (Rgb) {.r = rgba.r, .g = rgba.g, .b = rgba.b};
}

Rgba rgba_from_rgb(Rgb rgb, double alpha) {
    GeomVec3 clamped_rgb = geom_vec3_clamp(rgb_to_geom(rgb));
    double clamped_alpha = alpha < 0.0 ? 0.0 : (alpha > 1.0 ? 1.0 : alpha);
    return rgba_new(clamped_rgb.x, clamped_rgb.y, clamped_rgb.z, clamped_alpha);
}

Rgb rgb_from_geom(GeomVec3 vec) {
    return rgb_new(vec.x, vec.y, vec.z);
}

GeomVec3 rgb_to_geom(Rgb rgb) {
    return geom_vec3_new(rgb.r, rgb.g, rgb.b);
}

uint32_t rgba_pack(Rgba rgba) {
    return ((uint32_t)round(rgba.r * 255.0) << 24)
         | ((uint32_t)round(rgba.g * 255.0) << 16)
         | ((uint32_t)round(rgba.b * 255.0) << 8)
         | (uint32_t)round(rgba.a * 255.0);
}

Rgba rgba_unpack(uint32_t packed_rgba) {
    return rgba_new(
        (double)((packed_rgba >> 24) & 0xff) / 255.0,
        (double)((packed_rgba >> 16) & 0xff) / 255.0,
        (double)((packed_rgba >> 8) & 0xff) / 255.0,
        (double)(packed_rgba & 0xff) / 255.0
    );
}

Rgba rgba_blend_src_over(Rgba dst, Rgba src) {
    if (src.a <= 0.0) {
        return rgba_from_rgb(rgb_from_rgba(dst), dst.a);
    }
    if (src.a >= 1.0) {
        return rgba_from_rgb(rgb_from_rgba(src), src.a);
    }

    double out_a = src.a + dst.a * (1.0 - src.a);
    if (out_a <= 1e-12) {
        return rgba_new(0.0, 0.0, 0.0, 0.0);
    }

    Rgb src_rgb = rgb_from_rgba(src);
    Rgb dst_rgb = rgb_from_rgba(dst);

    GeomVec3 src_rgb_vec = rgb_to_geom(src_rgb);
    GeomVec3 dst_rgb_vec = rgb_to_geom(dst_rgb);

    GeomVec3 src_premul = geom_vec3_mul(src_rgb_vec, geom_vec3_scalar(src.a));
    GeomVec3 dst_premul =
        geom_vec3_mul(dst_rgb_vec, geom_vec3_scalar(dst.a * (1.0 - src.a)));
    GeomVec3 out_rgb = geom_vec3_div(
        geom_vec3_add(src_premul, dst_premul),
        geom_vec3_scalar(out_a)
    );

    return rgba_from_rgb(rgb_from_geom(out_rgb), out_a);
}

Rgb srgb_to_non_linear(Rgb linear_srgb, Rgb whitepoint, Rgb blackpoint) {
    GeomVec3 linear_vec = rgb_to_geom(linear_srgb);
    GeomVec3 whitepoint_vec = rgb_to_geom(whitepoint);
    GeomVec3 blackpoint_vec = rgb_to_geom(blackpoint);

    GeomVec3 low = geom_vec3_mul(linear_vec, geom_vec3_scalar(12.92));
    GeomVec3 high = geom_vec3_add(
        geom_vec3_mul(
            geom_vec3_scalar(1.055),
            geom_vec3_pow(linear_vec, geom_vec3_scalar(1.0 / 2.4))
        ),
        geom_vec3_scalar(-0.055)
    );

    GeomVec3 selected = geom_vec3_new(
        linear_vec.x <= 0.0031308 ? low.x : high.x,
        linear_vec.y <= 0.0031308 ? low.y : high.y,
        linear_vec.z <= 0.0031308 ? low.z : high.z
    );

    return rgb_from_geom(geom_vec3_add(
        geom_vec3_mul(geom_vec3_sub(whitepoint_vec, blackpoint_vec), selected),
        blackpoint_vec
    ));
}
