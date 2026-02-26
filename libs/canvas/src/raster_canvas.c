#include "raster_canvas.h"

#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "arena/common.h"
#include "canvas/canvas.h"
#include "dcel.h"
#include "logger/log.h"
#include "path_builder.h"

#define BMP_HEADER_LEN 14
#define BMP_INFO_HEADER_LEN 40

typedef struct {
    const PathBuilder* path;
    DcelFillRule fill_rule;
} ClipPathEntry;

#define DVEC_NAME ClipPathVec
#define DVEC_LOWERCASE_NAME clip_path_vec
#define DVEC_TYPE ClipPathEntry
#include "arena/dvec_impl.h"

#define DARRAY_NAME GeomVec2Array
#define DARRAY_LOWERCASE_NAME geom_vec2_array
#define DARRAY_TYPE GeomVec2
#include "arena/darray_impl.h"

struct RasterCanvas {
    Arena* arena;

    uint32_t width;
    uint32_t height;
    uint32_t file_size;
    uint8_t* data;

    double coordinate_scale;

    ClipPathVec* clip_paths;
};

static void write_u16(uint8_t* target, uint16_t value) {
    RELEASE_ASSERT(target);

    target[0] = (uint8_t)((value >> 0) & 0xff);
    target[1] = (uint8_t)((value >> 8) & 0xff);
}

static void write_u32(uint8_t* target, uint32_t value) {
    RELEASE_ASSERT(target);

    target[0] = (uint8_t)((value >> 0) & 0xff);
    target[1] = (uint8_t)((value >> 8) & 0xff);
    target[2] = (uint8_t)((value >> 16) & 0xff);
    target[3] = (uint8_t)((value >> 24) & 0xff);
}

static void write_bmp_header(uint8_t* target, uint32_t file_size) {
    RELEASE_ASSERT(target);

    // Header field
    target[0] = 'B';
    target[1] = 'M';

    // File size
    write_u32(target + 2, file_size);

    // Pixel data offset
    write_u32(target + 10, BMP_HEADER_LEN + BMP_INFO_HEADER_LEN);
}

static void
write_bmp_info_header(uint8_t* target, uint32_t width, uint32_t height) {
    RELEASE_ASSERT(target);
    RELEASE_ASSERT((int32_t)width >= 0); // the size is technically signed
    RELEASE_ASSERT((int32_t)height >= 0);

    write_u32(target, 40);         // header size
    write_u32(target + 4, width);  // width
    write_u32(target + 8, height); // height
    write_u16(target + 12, 1);     // color planes
    write_u16(target + 14, 32);    // bits per pixel
    write_u32(target + 16, 0);     // BI_RGB
    write_u32(target + 20, 0);     // image size, can be 0 for BI_RGB
}

RasterCanvas* raster_canvas_new(
    Arena* arena,
    uint32_t width,
    uint32_t height,
    Rgba rgba,
    double coordinate_scale
) {
    RELEASE_ASSERT(coordinate_scale > 1e-3);

    uint32_t file_size =
        BMP_HEADER_LEN + BMP_INFO_HEADER_LEN + width * height * 4;
    uint32_t packed_rgba = rgba_pack(rgba);

    LOG_DIAG(
        INFO,
        CANVAS,
        "Creating new %ux%u (%u bytes) canvas with initial color 0x%08" PRIx32,
        width,
        height,
        file_size,
        packed_rgba
    );

    RasterCanvas* canvas = arena_alloc(arena, sizeof(RasterCanvas));
    canvas->arena = arena;
    canvas->width = width;
    canvas->height = height;
    canvas->file_size = file_size;
    canvas->data = arena_alloc(arena, file_size);
    memset(canvas->data, 0, file_size);

    canvas->coordinate_scale = coordinate_scale;
    canvas->clip_paths = clip_path_vec_new(arena);

    write_bmp_header(canvas->data, file_size);
    write_bmp_info_header(canvas->data + BMP_HEADER_LEN, width, height);

    uint8_t r = (uint8_t)((packed_rgba >> 24) & 0xff);
    uint8_t g = (uint8_t)((packed_rgba >> 16) & 0xff);
    uint8_t b = (uint8_t)((packed_rgba >> 8) & 0xff);
    uint8_t a = (uint8_t)(packed_rgba & 0xff);
    for (uint32_t idx = 0; idx < width * height; idx++) {
        canvas->data[BMP_HEADER_LEN + BMP_INFO_HEADER_LEN + idx * 4] = b;
        canvas->data[BMP_HEADER_LEN + BMP_INFO_HEADER_LEN + idx * 4 + 1] = g;
        canvas->data[BMP_HEADER_LEN + BMP_INFO_HEADER_LEN + idx * 4 + 2] = r;
        canvas->data[BMP_HEADER_LEN + BMP_INFO_HEADER_LEN + idx * 4 + 3] = a;
    }

    return canvas;
}

double raster_canvas_raster_res(const RasterCanvas* canvas) {
    RELEASE_ASSERT(canvas);
    return 1.0 / canvas->coordinate_scale;
}

uint32_t raster_canvas_width(const RasterCanvas* canvas) {
    RELEASE_ASSERT(canvas);
    return canvas->width;
}

uint32_t raster_canvas_height(const RasterCanvas* canvas) {
    RELEASE_ASSERT(canvas);
    return canvas->height;
}

static bool raster_canvas_pixel_visible(
    const RasterCanvas* canvas,
    uint32_t x,
    uint32_t y
) {
    RELEASE_ASSERT(canvas);

    if (clip_path_vec_len(canvas->clip_paths) == 0) {
        return true;
    }

    double sample_x = ((double)x + 0.5) / canvas->coordinate_scale;
    double sample_y = ((double)y + 0.5) / canvas->coordinate_scale;
    for (size_t idx = 0; idx < clip_path_vec_len(canvas->clip_paths); idx++) {
        ClipPathEntry clip_path;
        RELEASE_ASSERT(clip_path_vec_get(canvas->clip_paths, idx, &clip_path));
        if (!dcel_path_contains_point(
                clip_path.path,
                clip_path.fill_rule,
                sample_x,
                sample_y
            )) {
            return false;
        }
    }

    return true;
}

Rgba raster_canvas_get_rgba(
    const RasterCanvas* canvas,
    uint32_t x,
    uint32_t y
) {
    RELEASE_ASSERT(canvas);
    RELEASE_ASSERT(x < canvas->width);
    RELEASE_ASSERT(y < canvas->height);

    size_t pixel_offset = BMP_HEADER_LEN + BMP_INFO_HEADER_LEN
                        + ((canvas->height - y - 1) * canvas->width + x) * 4;

    return rgba_unpack(
        ((uint32_t)canvas->data[pixel_offset + 2] << 24)
        | ((uint32_t)canvas->data[pixel_offset + 1] << 16)
        | ((uint32_t)canvas->data[pixel_offset + 0] << 8)
        | ((uint32_t)canvas->data[pixel_offset + 3])
    );
}

void raster_canvas_set_rgba(
    RasterCanvas* canvas,
    uint32_t x,
    uint32_t y,
    Rgba rgba
) {
    RELEASE_ASSERT(canvas);
    RELEASE_ASSERT(x < canvas->width);
    RELEASE_ASSERT(y < canvas->height);
    if (!raster_canvas_pixel_visible(canvas, x, y)) {
        return;
    }

    uint32_t packed_rgba = rgba_pack(rgba);

    LOG_DIAG(
        TRACE,
        CANVAS,
        "Setting canvas pixel (%u, %u) to 0x%08" PRIx32,
        x,
        y,
        packed_rgba
    );

    size_t pixel_offset = BMP_HEADER_LEN + BMP_INFO_HEADER_LEN
                        + ((canvas->height - y - 1) * canvas->width + x) * 4;

    canvas->data[pixel_offset + 2] = (uint8_t)((packed_rgba >> 24) & 0xff);
    canvas->data[pixel_offset + 1] = (uint8_t)((packed_rgba >> 16) & 0xff);
    canvas->data[pixel_offset + 0] = (uint8_t)((packed_rgba >> 8) & 0xff);
    canvas->data[pixel_offset + 3] = (uint8_t)(packed_rgba & 0xff);
}

static uint32_t clamp_and_floor(double value, uint32_t max) {
    if (value < 0) {
        return 0;
    }

    if (value > max) {
        return max;
    }

    return (uint32_t)value;
}

static uint32_t clamp_and_ceil(double value, uint32_t max) {
    if (value < 0) {
        return 0;
    }

    if (value > max) {
        return max;
    }

    return (uint32_t)ceil(value);
}

void raster_canvas_draw_circle(
    RasterCanvas* canvas,
    double x,
    double y,
    double radius,
    Rgba rgba
) {
    RELEASE_ASSERT(canvas);

    x *= canvas->coordinate_scale;
    y *= canvas->coordinate_scale;
    radius *= canvas->coordinate_scale;

    double min_x = x - radius;
    double min_y = y - radius;
    double max_x = x + radius;
    double max_y = y + radius;

    for (uint32_t current_y = clamp_and_floor(min_y, canvas->height - 1);
         current_y < clamp_and_ceil(max_y, canvas->height - 1);
         current_y++) {
        for (uint32_t current_x = clamp_and_floor(min_x, canvas->width - 1);
             current_x < clamp_and_ceil(max_x, canvas->width - 1);
             current_x++) {
            if (sqrt(
                    pow((double)current_x + 0.5 - x, 2.0)
                    + pow((double)current_y + 0.5 - y, 2.0)
                )
                > radius) {
                continue;
            }

            raster_canvas_set_rgba(canvas, current_x, current_y, rgba);
        }
    }
}

void raster_canvas_draw_line(
    RasterCanvas* canvas,
    double x1,
    double y1,
    double x2,
    double y2,
    double radius,
    Rgba rgba
) {
    RELEASE_ASSERT(canvas);

    double dx = x2 - x1;
    double dy = y2 - y1;
    double dist = sqrt(dx * dx + dy * dy);

    for (int pixel = 1; pixel < (int)dist; pixel++) {
        double t = pixel / dist;
        raster_canvas_draw_circle(
            canvas,
            x1 + dx * t,
            y1 + dy * t,
            radius,
            rgba
        );
    }
}

void raster_canvas_draw_arrow(
    RasterCanvas* canvas,
    double x1,
    double y1,
    double x2,
    double y2,
    double radius,
    double tip_radius,
    Rgba rgba
) {
    RELEASE_ASSERT(canvas);

    double dx = x2 - x1;
    double dy = y2 - y1;
    double dist = sqrt(dx * dx + dy * dy);

    for (int pixel = 1; pixel < (int)dist; pixel++) {
        double t = pixel / dist;
        raster_canvas_draw_circle(
            canvas,
            x1 + dx * t,
            y1 + dy * t,
            radius * (1.0 - t) + tip_radius * t,
            rgba
        );
    }
}

void raster_canvas_draw_bezier(
    RasterCanvas* canvas,
    double x1,
    double y1,
    double x2,
    double y2,
    double cx,
    double cy,
    double flatness,
    double radius,
    Rgba rgba
) {
    RELEASE_ASSERT(canvas);

    raster_canvas_draw_circle(canvas, x1, y1, radius * 3.0, rgba);
    raster_canvas_draw_circle(canvas, x2, y2, radius * 3.0, rgba);

    // Check if line is flat
    double mid_x = (x1 + x2) / 2.0;
    double mid_y = (y1 + y2) / 2.0;
    double flatness_x = cx - mid_x;
    double flatness_y = cy - mid_y;
    if (sqrt(flatness_x * flatness_x + flatness_y * flatness_y) < flatness) {
        raster_canvas_draw_line(canvas, x1, y1, x2, y2, radius, rgba);
        return;
    }

    // Recurse
    double c1x = (x1 + cx) / 2.0;
    double c1y = (y1 + cy) / 2.0;
    double c2x = (x2 + cx) / 2.0;
    double c2y = (y2 + cy) / 2.0;

    double xm = (c1x + c2x) * 0.5;
    double ym = (c1y + c2y) * 0.5;

    raster_canvas_draw_bezier(
        canvas,
        x1,
        y1,
        xm,
        ym,
        c1x,
        c1y,
        flatness,
        radius,
        rgba
    );
    raster_canvas_draw_bezier(
        canvas,
        xm,
        ym,
        x2,
        y2,
        c2x,
        c2y,
        flatness,
        radius,
        rgba
    );
}

static void raster_canvas_append_arc(
    PathBuilder* outline,
    GeomVec2 center,
    GeomVec2 from,
    GeomVec2 to,
    bool ccw
) {
    RELEASE_ASSERT(outline);

    double angle_from = geom_vec2_angle(center, from);
    double angle_to = geom_vec2_angle(center, to);
    if (ccw) {
        while (angle_to <= angle_from) {
            angle_to += 2.0 * M_PI;
        }
    } else {
        while (angle_to >= angle_from) {
            angle_to -= 2.0 * M_PI;
        }
    }

    double angle_span = fabs(angle_to - angle_from);
    size_t segment_count = (size_t)ceil(angle_span * 8.0 / M_PI);
    if (segment_count < 2) {
        segment_count = 2;
    } else if (segment_count > 32) {
        segment_count = 32;
    }

    double arc_radius = sqrt(
        (from.x - center.x) * (from.x - center.x)
        + (from.y - center.y) * (from.y - center.y)
    );
    for (size_t segment_idx = 1; segment_idx <= segment_count; segment_idx++) {
        double interpolation_factor =
            (double)segment_idx / (double)segment_count;
        double angle =
            angle_from + interpolation_factor * (angle_to - angle_from);
        path_builder_line_to(
            outline,
            geom_vec2_new(
                center.x + arc_radius * cos(angle),
                center.y + arc_radius * sin(angle)
            )
        );
    }
}

static GeomVec2 raster_canvas_compute_inner_join(
    GeomVec2 from,
    GeomVec2 direction_from,
    GeomVec2 to,
    GeomVec2 direction_to,
    GeomVec2 fallback
) {
    double determinant = geom_vec2_cross(direction_from, direction_to);
    if (fabs(determinant) < 1e-12) {
        return fallback;
    }

    GeomVec2 delta = geom_vec2_sub(to, from);
    double intersection_factor =
        (delta.x * direction_to.y - delta.y * direction_to.x) / determinant;
    return geom_vec2_add(
        from,
        geom_vec2_scale(direction_from, intersection_factor)
    );
}

static void raster_canvas_append_outer_join(
    PathBuilder* outline,
    GeomVec2 vertex,
    GeomVec2 from,
    GeomVec2 direction_from,
    GeomVec2 to,
    GeomVec2 direction_to,
    double stroke_radius,
    CanvasLineJoin line_join,
    double miter_limit
) {
    RELEASE_ASSERT(outline);

    switch (line_join) {
        case CANVAS_LINEJOIN_BEVEL: {
            path_builder_line_to(outline, to);
            break;
        }
        case CANVAS_LINEJOIN_MITER: {
            double determinant = geom_vec2_cross(direction_from, direction_to);
            if (fabs(determinant) > 1e-12) {
                GeomVec2 delta = geom_vec2_sub(to, from);
                double intersection_factor =
                    (delta.x * direction_to.y - delta.y * direction_to.x)
                    / determinant;
                GeomVec2 miter_point = geom_vec2_add(
                    from,
                    geom_vec2_scale(direction_from, intersection_factor)
                );
                double miter_distance_sq =
                    geom_vec2_len_sq(geom_vec2_sub(miter_point, vertex));
                if (miter_distance_sq <= miter_limit * miter_limit
                                             * stroke_radius * stroke_radius) {
                    path_builder_line_to(outline, miter_point);
                    path_builder_line_to(outline, to);
                    break;
                }
            }

            path_builder_line_to(outline, to);
            break;
        }
        case CANVAS_LINEJOIN_ROUND: {
            raster_canvas_append_arc(outline, vertex, from, to, true);
            break;
        }
    }
}

static void raster_canvas_append_join(
    PathBuilder* outline,
    GeomVec2 vertex,
    GeomVec2 from,
    GeomVec2 direction_from,
    GeomVec2 to,
    GeomVec2 direction_to,
    double cross_product,
    int side_sign,
    double stroke_radius,
    CanvasLineJoin line_join,
    double miter_limit
) {
    RELEASE_ASSERT(outline);
    RELEASE_ASSERT(side_sign == 1 || side_sign == -1);

    if (fabs(cross_product) < 1e-10) {
        path_builder_line_to(outline, to);
        return;
    }

    bool outer_join = side_sign > 0 ? cross_product > 0 : cross_product < 0;
    if (outer_join) {
        raster_canvas_append_outer_join(
            outline,
            vertex,
            from,
            direction_from,
            to,
            direction_to,
            stroke_radius,
            line_join,
            miter_limit
        );
    } else {
        GeomVec2 inner_point = raster_canvas_compute_inner_join(
            from,
            direction_from,
            to,
            direction_to,
            vertex
        );
        path_builder_line_to(outline, inner_point);
        path_builder_line_to(outline, to);
    }
}

static void raster_canvas_build_open_stroke_outline(
    PathBuilder* outline,
    Arena* arena,
    const GeomVec2Array* points,
    size_t point_count,
    double stroke_radius,
    CanvasLineCap line_cap,
    CanvasLineJoin line_join,
    double miter_limit
) {
    RELEASE_ASSERT(outline);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(points);
    RELEASE_ASSERT(point_count >= 2);
    RELEASE_ASSERT(geom_vec2_array_len(points) >= point_count);

    size_t segment_count = point_count - 1;
    GeomVec2Array* directions = geom_vec2_array_new(arena, segment_count);
    GeomVec2Array* normals = geom_vec2_array_new(arena, segment_count);

    for (size_t segment_idx = 0; segment_idx < segment_count; segment_idx++) {
        GeomVec2 point_from;
        GeomVec2 point_to;
        RELEASE_ASSERT(geom_vec2_array_get(points, segment_idx, &point_from));
        RELEASE_ASSERT(geom_vec2_array_get(points, segment_idx + 1, &point_to));

        GeomVec2 direction =
            geom_vec2_normalize(geom_vec2_sub(point_to, point_from));
        geom_vec2_array_set(directions, segment_idx, direction);
        geom_vec2_array_set(
            normals,
            segment_idx,
            geom_vec2_perpendicular(direction)
        );
    }

    GeomVec2 first_point;
    GeomVec2 first_normal;
    GeomVec2 first_direction;
    RELEASE_ASSERT(geom_vec2_array_get(points, 0, &first_point));
    RELEASE_ASSERT(geom_vec2_array_get(normals, 0, &first_normal));
    RELEASE_ASSERT(geom_vec2_array_get(directions, 0, &first_direction));
    GeomVec2 start_left = geom_vec2_add(
        first_point,
        geom_vec2_scale(first_normal, stroke_radius)
    );
    if (line_cap == CANVAS_LINECAP_SQUARE) {
        start_left = geom_vec2_sub(
            start_left,
            geom_vec2_scale(first_direction, stroke_radius)
        );
    }
    path_builder_new_contour(outline, start_left);

    GeomVec2 curr_left = start_left;
    for (size_t segment_idx = 0; segment_idx < segment_count; segment_idx++) {
        GeomVec2 point_to;
        GeomVec2 segment_normal;
        GeomVec2 segment_direction;
        RELEASE_ASSERT(geom_vec2_array_get(points, segment_idx + 1, &point_to));
        RELEASE_ASSERT(
            geom_vec2_array_get(normals, segment_idx, &segment_normal)
        );
        RELEASE_ASSERT(
            geom_vec2_array_get(directions, segment_idx, &segment_direction)
        );
        GeomVec2 end_left = geom_vec2_add(
            point_to,
            geom_vec2_scale(segment_normal, stroke_radius)
        );

        if (line_cap == CANVAS_LINECAP_SQUARE
            && segment_idx == segment_count - 1) {
            end_left = geom_vec2_add(
                end_left,
                geom_vec2_scale(segment_direction, stroke_radius)
            );
        }

        path_builder_line_to(outline, end_left);
        curr_left = end_left;

        if (segment_idx + 1 < segment_count) {
            GeomVec2 next_normal;
            GeomVec2 next_direction;
            RELEASE_ASSERT(
                geom_vec2_array_get(normals, segment_idx + 1, &next_normal)
            );
            RELEASE_ASSERT(geom_vec2_array_get(
                directions,
                segment_idx + 1,
                &next_direction
            ));
            GeomVec2 next_left = geom_vec2_add(
                point_to,
                geom_vec2_scale(next_normal, stroke_radius)
            );
            double cross_product =
                geom_vec2_cross(segment_direction, next_direction);
            raster_canvas_append_join(
                outline,
                point_to,
                curr_left,
                segment_direction,
                next_left,
                next_direction,
                cross_product,
                1,
                stroke_radius,
                line_join,
                miter_limit
            );
            curr_left = next_left;
        }
    }

    GeomVec2 end_point;
    GeomVec2 end_normal;
    GeomVec2 end_direction;
    RELEASE_ASSERT(geom_vec2_array_get(points, segment_count, &end_point));
    RELEASE_ASSERT(
        geom_vec2_array_get(normals, segment_count - 1, &end_normal)
    );
    RELEASE_ASSERT(
        geom_vec2_array_get(directions, segment_count - 1, &end_direction)
    );
    GeomVec2 end_right =
        geom_vec2_sub(end_point, geom_vec2_scale(end_normal, stroke_radius));
    if (line_cap == CANVAS_LINECAP_SQUARE) {
        end_right = geom_vec2_add(
            end_right,
            geom_vec2_scale(end_direction, stroke_radius)
        );
    }

    if (line_cap == CANVAS_LINECAP_ROUND) {
        raster_canvas_append_arc(
            outline,
            end_point,
            curr_left,
            end_right,
            false
        );
    } else {
        path_builder_line_to(outline, end_right);
    }

    GeomVec2 curr_right = end_right;
    for (size_t reverse_segment_idx = segment_count; reverse_segment_idx > 0;
         reverse_segment_idx--) {
        size_t segment_idx = reverse_segment_idx - 1;
        GeomVec2 point_curr;
        GeomVec2 normal_curr;
        RELEASE_ASSERT(geom_vec2_array_get(points, segment_idx, &point_curr));
        RELEASE_ASSERT(geom_vec2_array_get(normals, segment_idx, &normal_curr));
        GeomVec2 prev_right = geom_vec2_sub(
            point_curr,
            geom_vec2_scale(normal_curr, stroke_radius)
        );

        if (line_cap == CANVAS_LINECAP_SQUARE && segment_idx == 0) {
            GeomVec2 start_direction;
            RELEASE_ASSERT(
                geom_vec2_array_get(directions, 0, &start_direction)
            );
            prev_right = geom_vec2_sub(
                prev_right,
                geom_vec2_scale(start_direction, stroke_radius)
            );
        }
        path_builder_line_to(outline, prev_right);
        curr_right = prev_right;

        if (segment_idx > 0) {
            GeomVec2 normal_prev;
            GeomVec2 direction_prev;
            GeomVec2 direction_curr;
            RELEASE_ASSERT(
                geom_vec2_array_get(normals, segment_idx - 1, &normal_prev)
            );
            RELEASE_ASSERT(geom_vec2_array_get(
                directions,
                segment_idx - 1,
                &direction_prev
            ));
            RELEASE_ASSERT(
                geom_vec2_array_get(directions, segment_idx, &direction_curr)
            );
            GeomVec2 next_right = geom_vec2_sub(
                point_curr,
                geom_vec2_scale(normal_prev, stroke_radius)
            );
            double cross_product =
                geom_vec2_cross(direction_prev, direction_curr);
            raster_canvas_append_join(
                outline,
                point_curr,
                curr_right,
                geom_vec2_scale(direction_curr, -1.0),
                next_right,
                geom_vec2_scale(direction_prev, -1.0),
                cross_product,
                -1,
                stroke_radius,
                line_join,
                miter_limit
            );
            curr_right = next_right;
        }
    }

    if (line_cap == CANVAS_LINECAP_ROUND) {
        raster_canvas_append_arc(
            outline,
            first_point,
            curr_right,
            start_left,
            false
        );
    }

    path_builder_close_contour(outline);
}

static void raster_canvas_build_closed_stroke_outline(
    PathBuilder* outline,
    Arena* arena,
    const GeomVec2Array* points,
    size_t point_count,
    double stroke_radius,
    CanvasLineJoin line_join,
    double miter_limit
) {
    RELEASE_ASSERT(outline);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(points);
    RELEASE_ASSERT(point_count >= 3);
    RELEASE_ASSERT(geom_vec2_array_len(points) >= point_count);

    GeomVec2Array* directions = geom_vec2_array_new(arena, point_count);
    GeomVec2Array* normals = geom_vec2_array_new(arena, point_count);

    for (size_t point_idx = 0; point_idx < point_count; point_idx++) {
        size_t next_point_idx = (point_idx + 1) % point_count;
        GeomVec2 point_from;
        GeomVec2 point_to;
        RELEASE_ASSERT(geom_vec2_array_get(points, point_idx, &point_from));
        RELEASE_ASSERT(geom_vec2_array_get(points, next_point_idx, &point_to));
        GeomVec2 direction =
            geom_vec2_normalize(geom_vec2_sub(point_to, point_from));
        geom_vec2_array_set(directions, point_idx, direction);
        geom_vec2_array_set(
            normals,
            point_idx,
            geom_vec2_perpendicular(direction)
        );
    }

    for (size_t side_idx = 0; side_idx < 2; side_idx++) {
        int side_sign = side_idx == 0 ? 1 : -1;
        GeomVec2 first_point;
        GeomVec2 first_normal;
        RELEASE_ASSERT(geom_vec2_array_get(points, 0, &first_point));
        RELEASE_ASSERT(geom_vec2_array_get(normals, 0, &first_normal));
        GeomVec2 start_point =
            side_sign > 0 ? geom_vec2_add(
                                first_point,
                                geom_vec2_scale(first_normal, stroke_radius)
                            )
                          : geom_vec2_sub(
                                first_point,
                                geom_vec2_scale(first_normal, stroke_radius)
                            );
        path_builder_new_contour(outline, start_point);

        GeomVec2 curr_point = start_point;
        for (size_t point_idx = 0; point_idx < point_count; point_idx++) {
            size_t next_point_idx = (point_idx + 1) % point_count;
            GeomVec2 next_point;
            GeomVec2 curr_normal;
            GeomVec2 next_normal;
            GeomVec2 curr_direction;
            GeomVec2 next_direction;
            RELEASE_ASSERT(
                geom_vec2_array_get(points, next_point_idx, &next_point)
            );
            RELEASE_ASSERT(
                geom_vec2_array_get(normals, point_idx, &curr_normal)
            );
            RELEASE_ASSERT(
                geom_vec2_array_get(normals, next_point_idx, &next_normal)
            );
            RELEASE_ASSERT(
                geom_vec2_array_get(directions, point_idx, &curr_direction)
            );
            RELEASE_ASSERT(
                geom_vec2_array_get(directions, next_point_idx, &next_direction)
            );
            GeomVec2 segment_end_point =
                side_sign > 0 ? geom_vec2_add(
                                    next_point,
                                    geom_vec2_scale(curr_normal, stroke_radius)
                                )
                              : geom_vec2_sub(
                                    next_point,
                                    geom_vec2_scale(curr_normal, stroke_radius)
                                );
            path_builder_line_to(outline, segment_end_point);
            curr_point = segment_end_point;

            GeomVec2 join_point =
                side_sign > 0 ? geom_vec2_add(
                                    next_point,
                                    geom_vec2_scale(next_normal, stroke_radius)
                                )
                              : geom_vec2_sub(
                                    next_point,
                                    geom_vec2_scale(next_normal, stroke_radius)
                                );
            double cross_product =
                geom_vec2_cross(curr_direction, next_direction);
            raster_canvas_append_join(
                outline,
                next_point,
                curr_point,
                curr_direction,
                join_point,
                next_direction,
                cross_product,
                side_sign,
                stroke_radius,
                line_join,
                miter_limit
            );
            curr_point = join_point;
        }

        path_builder_close_contour(outline);
    }
}

void raster_canvas_draw_path(
    RasterCanvas* canvas,
    const PathBuilder* path,
    CanvasBrush brush
) {
    RELEASE_ASSERT(canvas);
    RELEASE_ASSERT(path);
    if (canvas->width == 0 || canvas->height == 0) {
        return;
    }

    if (brush.enable_fill) {
        Arena* local_arena = arena_new(4096);
        size_t pixel_count = (size_t)canvas->width * (size_t)canvas->height;
        Uint8Array* mask = uint8_array_new(local_arena, pixel_count);
        DcelMaskBounds bounds;

        dcel_rasterize_path_mask(
            local_arena,
            path,
            brush.even_odd_fill ? DCEL_FILL_RULE_EVEN_ODD
                                : DCEL_FILL_RULE_NONZERO,
            canvas->width,
            canvas->height,
            canvas->coordinate_scale,
            mask,
            &bounds
        );

        if (!bounds.is_empty) {
            for (uint32_t y = bounds.min_y;; y++) {
                for (uint32_t x = bounds.min_x;; x++) {
                    size_t mask_idx =
                        (size_t)y * (size_t)canvas->width + (size_t)x;
                    uint8_t mask_value = 0;
                    RELEASE_ASSERT(
                        uint8_array_get(mask, mask_idx, &mask_value)
                    );
                    if (mask_value != 0) {
                        Rgba dst = raster_canvas_get_rgba(canvas, x, y);
                        Rgba out = rgba_blend_src_over(dst, brush.fill_rgba);
                        raster_canvas_set_rgba(canvas, x, y, out);
                    }

                    if (x == bounds.max_x) {
                        break;
                    }
                }

                if (y == bounds.max_y) {
                    break;
                }
            }
        }

        arena_free(local_arena);
    }

    if (!brush.enable_stroke || brush.stroke_width <= 0.0) {
        return;
    }

    double stroke_radius = brush.stroke_width * 0.5;

    for (size_t contour_idx = 0;
         contour_idx < path_contour_vec_len(path->contours);
         contour_idx++) {
        PathContour* contour = NULL;
        RELEASE_ASSERT(
            path_contour_vec_get(path->contours, contour_idx, &contour)
        );

        if (path_contour_len(contour) < 2) {
            continue;
        }

        Arena* stroke_arena = arena_new(4096);

        size_t max_point_count = path_contour_len(contour);
        GeomVec2Array* points =
            geom_vec2_array_new(stroke_arena, max_point_count);
        size_t point_count = 0;

        PathContourSegment first_segment;
        RELEASE_ASSERT(path_contour_get(contour, 0, &first_segment));
        RELEASE_ASSERT(first_segment.type == PATH_CONTOUR_SEGMENT_TYPE_START);
        geom_vec2_array_set(points, point_count++, first_segment.value.start);

        for (size_t segment_idx = 1; segment_idx < path_contour_len(contour);
             segment_idx++) {
            PathContourSegment segment;
            RELEASE_ASSERT(path_contour_get(contour, segment_idx, &segment));
            RELEASE_ASSERT(
                segment.type == PATH_CONTOUR_SEGMENT_TYPE_LINE,
                "Raster stroke requires flattened curves. Use "
                "path_builder_options_flattened()."
            );
            GeomVec2 point = segment.value.line;
            GeomVec2 previous_point;
            RELEASE_ASSERT(
                geom_vec2_array_get(points, point_count - 1, &previous_point)
            );
            if (!geom_vec2_equal_eps(point, previous_point, 1e-12)) {
                geom_vec2_array_set(points, point_count++, point);
            }
        }

        bool closed = false;
        if (point_count >= 2) {
            GeomVec2 first_point;
            GeomVec2 last_point;
            RELEASE_ASSERT(geom_vec2_array_get(points, 0, &first_point));
            RELEASE_ASSERT(
                geom_vec2_array_get(points, point_count - 1, &last_point)
            );
            closed = geom_vec2_equal_eps(first_point, last_point, 1e-9);
        }
        if (closed) {
            point_count--;
        }

        bool can_stroke = closed ? point_count >= 3 : point_count >= 2;
        if (can_stroke) {
            PathBuilder* stroke_outline = path_builder_new_with_options(
                stroke_arena,
                path_builder_options_flattened()
            );
            if (closed) {
                raster_canvas_build_closed_stroke_outline(
                    stroke_outline,
                    stroke_arena,
                    points,
                    point_count,
                    stroke_radius,
                    brush.line_join,
                    brush.miter_limit
                );
            } else {
                raster_canvas_build_open_stroke_outline(
                    stroke_outline,
                    stroke_arena,
                    points,
                    point_count,
                    stroke_radius,
                    brush.line_cap,
                    brush.line_join,
                    brush.miter_limit
                );
            }

            size_t pixel_count = (size_t)canvas->width * (size_t)canvas->height;
            Uint8Array* mask = uint8_array_new(stroke_arena, pixel_count);
            DcelMaskBounds bounds;

            dcel_rasterize_path_mask(
                stroke_arena,
                stroke_outline,
                DCEL_FILL_RULE_EVEN_ODD,
                canvas->width,
                canvas->height,
                canvas->coordinate_scale,
                mask,
                &bounds
            );

            if (!bounds.is_empty) {
                for (uint32_t y = bounds.min_y;; y++) {
                    for (uint32_t x = bounds.min_x;; x++) {
                        size_t mask_idx =
                            (size_t)y * (size_t)canvas->width + (size_t)x;
                        uint8_t mask_value = 0;
                        RELEASE_ASSERT(
                            uint8_array_get(mask, mask_idx, &mask_value)
                        );
                        if (mask_value != 0) {
                            Rgba dst = raster_canvas_get_rgba(canvas, x, y);
                            Rgba out =
                                rgba_blend_src_over(dst, brush.stroke_rgba);
                            raster_canvas_set_rgba(canvas, x, y, out);
                        }

                        if (x == bounds.max_x) {
                            break;
                        }
                    }

                    if (y == bounds.max_y) {
                        break;
                    }
                }
            }
        }

        arena_free(stroke_arena);
    }
}

void raster_canvas_push_clip_path(
    RasterCanvas* canvas,
    const PathBuilder* path,
    bool even_odd_rule
) {
    RELEASE_ASSERT(canvas);
    RELEASE_ASSERT(path);

    clip_path_vec_push(
        canvas->clip_paths,
        (ClipPathEntry) {.path = path,
                         .fill_rule = even_odd_rule ? DCEL_FILL_RULE_EVEN_ODD
                                                    : DCEL_FILL_RULE_NONZERO}
    );
}

void raster_canvas_pop_clip_paths(RasterCanvas* canvas, size_t count) {
    RELEASE_ASSERT(canvas);
    RELEASE_ASSERT(count <= clip_path_vec_len(canvas->clip_paths));

    for (size_t idx = 0; idx < count; idx++) {
        ClipPathEntry removed_path;
        RELEASE_ASSERT(clip_path_vec_pop(canvas->clip_paths, &removed_path));
    }
}

void raster_canvas_draw_pixel(
    RasterCanvas* canvas,
    GeomVec2 position,
    Rgba rgba
) {
    RELEASE_ASSERT(canvas);

    int64_t x = (int64_t)floor(position.x * canvas->coordinate_scale);
    int64_t y = (int64_t)floor(position.y * canvas->coordinate_scale);
    if (x < 0 || y < 0 || x >= (int64_t)canvas->width
        || y >= (int64_t)canvas->height) {
        return;
    }

    uint32_t px = (uint32_t)x;
    uint32_t py = (uint32_t)y;
    Rgba dst = raster_canvas_get_rgba(canvas, px, py);
    raster_canvas_set_rgba(canvas, px, py, rgba_blend_src_over(dst, rgba));
}

bool raster_canvas_write_file(RasterCanvas* canvas, const char* path) {
    RELEASE_ASSERT(canvas);
    RELEASE_ASSERT(path);

    LOG_DIAG(INFO, CANVAS, "Writing canvas to `%s`", path);

    FILE* file = fopen(path, "wb");
    if (!file) {
        return false;
    }

    unsigned long written =
        (unsigned long)fwrite(canvas->data, 1, canvas->file_size, file);
    fclose(file);

    return written == canvas->file_size;
}
