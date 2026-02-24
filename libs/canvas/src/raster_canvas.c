#include "raster_canvas.h"

#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "arena/arena.h"
#include "canvas/canvas.h"
#include "logger/log.h"

#define BMP_HEADER_LEN 14
#define BMP_INFO_HEADER_LEN 40

struct RasterCanvas {
    uint32_t width;
    uint32_t height;
    uint32_t file_size;
    uint8_t* data;

    double coordinate_scale;
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
    canvas->width = width;
    canvas->height = height;
    canvas->file_size = file_size;
    canvas->data = arena_alloc(arena, file_size);
    memset(canvas->data, 0, file_size);

    canvas->coordinate_scale = coordinate_scale;

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

uint32_t raster_canvas_width(const RasterCanvas* canvas) {
    RELEASE_ASSERT(canvas);
    return canvas->width;
}

uint32_t raster_canvas_height(const RasterCanvas* canvas) {
    RELEASE_ASSERT(canvas);
    return canvas->height;
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

void raster_canvas_draw_path(
    RasterCanvas* canvas,
    const PathBuilder* path,
    CanvasBrush brush
) {
    (void)brush;

    RELEASE_ASSERT(canvas);
    RELEASE_ASSERT(path);

    LOG_TODO();
}

void raster_canvas_push_clip_path(
    RasterCanvas* canvas,
    const PathBuilder* path,
    bool even_odd_rule
) {
    (void)canvas;
    (void)path;
    (void)even_odd_rule;

    LOG_TODO();
}

void raster_canvas_pop_clip_paths(RasterCanvas* canvas, size_t count) {
    (void)canvas;
    (void)count;

    LOG_TODO();
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
