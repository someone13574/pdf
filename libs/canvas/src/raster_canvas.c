#include "raster_canvas.h"

#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "arena/arena.h"
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

    uint32_t pixel_offset = BMP_HEADER_LEN + BMP_INFO_HEADER_LEN
                          + ((canvas->height - y - 1) * canvas->width + x) * 4;

    return rgba_unpack(
        ((uint32_t)canvas->data[pixel_offset + 2] << 24)
        | ((uint32_t)canvas->data[pixel_offset + 1] << 16)
        | ((uint32_t)canvas->data[pixel_offset] << 8)
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

    uint32_t pixel_offset = BMP_HEADER_LEN + BMP_INFO_HEADER_LEN
                          + ((canvas->height - y - 1) * canvas->width + x) * 4;

    canvas->data[pixel_offset + 2] = (uint8_t)((packed_rgba >> 24) & 0xff);
    canvas->data[pixel_offset + 1] = (uint8_t)((packed_rgba >> 16) & 0xff);
    canvas->data[pixel_offset] = (uint8_t)((packed_rgba >> 8) & 0xff);
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

typedef struct {
    RasterCanvas* canvas;
    double radius;
    Rgba rgba;
} RasterStrokeLineCtx;

static void raster_canvas_stroke_line_segment(
    RasterStrokeLineCtx* ctx,
    GeomVec2 from,
    GeomVec2 to
) {
    RELEASE_ASSERT(ctx);

    raster_canvas_draw_circle(
        ctx->canvas,
        from.x,
        from.y,
        ctx->radius,
        ctx->rgba
    );
    raster_canvas_draw_line(
        ctx->canvas,
        from.x,
        from.y,
        to.x,
        to.y,
        ctx->radius,
        ctx->rgba
    );
    raster_canvas_draw_circle(ctx->canvas, to.x, to.y, ctx->radius, ctx->rgba);
}

void raster_canvas_draw_path(
    RasterCanvas* canvas,
    const PathBuilder* path,
    CanvasBrush brush
) {
    RELEASE_ASSERT(canvas);
    RELEASE_ASSERT(path);

    if (brush.enable_fill) {
        Arena* local_arena = arena_new(4096);
        size_t pixel_count = (size_t)canvas->width * (size_t)canvas->height;
        uint8_t* mask = arena_alloc(local_arena, pixel_count * sizeof(uint8_t));
        DcelMaskBounds bounds;

        dcel_rasterize_path_mask(
            local_arena,
            path,
            brush.even_odd_fill ? DCEL_FILL_RULE_EVEN_ODD : DCEL_FILL_RULE_NONZERO,
            canvas->width,
            canvas->height,
            canvas->coordinate_scale,
            mask,
            &bounds
        );

        if (!bounds.is_empty) {
            for (uint32_t y = bounds.min_y;; y++) {
                for (uint32_t x = bounds.min_x;; x++) {
                    size_t idx = (size_t)y * (size_t)canvas->width + (size_t)x;
                    if (mask[idx] != 0) {
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

    // Join/cap styles are not implemented yet.
    (void)brush.line_cap;
    (void)brush.line_join;
    (void)brush.miter_limit;

    RasterStrokeLineCtx stroke_ctx = {
        .canvas = canvas,
        .radius = brush.stroke_width * 0.5,
        .rgba = brush.stroke_rgba
    };

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

        PathContourSegment first;
        RELEASE_ASSERT(path_contour_get(contour, 0, &first));
        RELEASE_ASSERT(first.type == PATH_CONTOUR_SEGMENT_TYPE_START);

        GeomVec2 current = first.value.start;

        for (size_t segment_idx = 1; segment_idx < path_contour_len(contour);
             segment_idx++) {
            PathContourSegment segment;
            RELEASE_ASSERT(path_contour_get(contour, segment_idx, &segment));

            switch (segment.type) {
                case PATH_CONTOUR_SEGMENT_TYPE_START: {
                    current = segment.value.start;
                    break;
                }
                case PATH_CONTOUR_SEGMENT_TYPE_LINE: {
                    raster_canvas_stroke_line_segment(
                        &stroke_ctx,
                        current,
                        segment.value.line
                    );
                    current = segment.value.line;
                    break;
                }
                case PATH_CONTOUR_SEGMENT_TYPE_QUAD_BEZIER: {
                    RELEASE_ASSERT(
                        false,
                        "Raster path requires flattened curves. Use path_builder_options_flattened()."
                    );
                    break;
                }
                case PATH_CONTOUR_SEGMENT_TYPE_CUBIC_BEZIER: {
                    RELEASE_ASSERT(
                        false,
                        "Raster path requires flattened curves. Use path_builder_options_flattened()."
                    );
                    break;
                }
            }
        }
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
