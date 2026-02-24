#include "canvas/canvas.h"

#include "arena/arena.h"
#include "canvas.h"
#include "logger/log.h"
#include "raster_canvas.h"
#include "scalable_canvas.h"

struct Canvas {
    union {
        RasterCanvas* raster;
        ScalableCanvas* scalable;
    } data;

    enum { CANVAS_TYPE_SCALABLE, CANVAS_TYPE_RASTER } type;
};

Canvas* canvas_new_raster(
    Arena* arena,
    uint32_t width,
    uint32_t height,
    Rgba rgba,
    double coordinate_scale
) {
    Canvas* canvas = arena_alloc(arena, sizeof(Canvas));
    canvas->data.raster =
        raster_canvas_new(arena, width, height, rgba, coordinate_scale);
    canvas->type = CANVAS_TYPE_RASTER;

    return canvas;
}

Canvas* canvas_from_raster(Arena* arena, RasterCanvas* raster_canvas) {
    Canvas* canvas = arena_alloc(arena, sizeof(Canvas));
    canvas->data.raster = raster_canvas;
    canvas->type = CANVAS_TYPE_RASTER;

    return canvas;
}

Canvas* canvas_new_scalable(
    Arena* arena,
    uint32_t width,
    uint32_t height,
    Rgba rgba,
    double raster_res
) {
    Canvas* canvas = arena_alloc(arena, sizeof(Canvas));
    canvas->data.scalable =
        scalable_canvas_new(arena, width, height, rgba, raster_res);
    canvas->type = CANVAS_TYPE_SCALABLE;

    return canvas;
}

bool canvas_is_raster(Canvas* canvas) {
    RELEASE_ASSERT(canvas);
    return canvas->type == CANVAS_TYPE_RASTER;
}

double canvas_raster_res(Canvas* canvas) {
    RELEASE_ASSERT(canvas);
    if (canvas->type == CANVAS_TYPE_RASTER) {
        return 1.0;
    } else {
        return scalable_canvas_raster_res(canvas->data.scalable);
    }
}

void canvas_draw_circle(
    Canvas* canvas,
    double x,
    double y,
    double radius,
    Rgba rgba
) {
    switch (canvas->type) {
        case CANVAS_TYPE_RASTER: {
            raster_canvas_draw_circle(canvas->data.raster, x, y, radius, rgba);
            break;
        }
        case CANVAS_TYPE_SCALABLE: {
            scalable_canvas_draw_circle(
                canvas->data.scalable,
                x,
                y,
                radius,
                rgba
            );
            break;
        }
        default: {
            LOG_PANIC("Unreachable");
        }
    }
}

void canvas_draw_line(
    Canvas* canvas,
    double x1,
    double y1,
    double x2,
    double y2,
    double radius,
    Rgba rgba
) {
    switch (canvas->type) {
        case CANVAS_TYPE_RASTER: {
            raster_canvas_draw_line(
                canvas->data.raster,
                x1,
                y1,
                x2,
                y2,
                radius,
                rgba
            );
            break;
        }
        case CANVAS_TYPE_SCALABLE: {
            scalable_canvas_draw_line(
                canvas->data.scalable,
                x1,
                y1,
                x2,
                y2,
                radius,
                rgba
            );
            break;
        }
        default: {
            LOG_PANIC("Unreachable");
        }
    }
}

void canvas_draw_bezier(
    Canvas* canvas,
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
    switch (canvas->type) {
        case CANVAS_TYPE_RASTER: {
            raster_canvas_draw_bezier(
                canvas->data.raster,
                x1,
                y1,
                x2,
                y2,
                cx,
                cy,
                flatness,
                radius,
                rgba
            );
            break;
        }
        case CANVAS_TYPE_SCALABLE: {
            scalable_canvas_draw_bezier(
                canvas->data.scalable,
                x1,
                y1,
                x2,
                y2,
                cx,
                cy,
                radius,
                rgba
            );
            break;
        }
        default: {
            LOG_PANIC("Unreachable");
        }
    }
}

void canvas_draw_path(
    Canvas* canvas,
    const PathBuilder* path,
    CanvasBrush brush
) {
    RELEASE_ASSERT(canvas);
    RELEASE_ASSERT(path);

    switch (canvas->type) {
        case CANVAS_TYPE_RASTER: {
            raster_canvas_draw_path(canvas->data.raster, path, brush);
            break;
        }
        case CANVAS_TYPE_SCALABLE: {
            scalable_canvas_draw_path(canvas->data.scalable, path, brush);
            break;
        }
        default: {
            LOG_PANIC("Unreachable");
        }
    }
}

void canvas_push_clip_path(
    Canvas* canvas,
    const PathBuilder* path,
    bool even_odd_rule
) {
    RELEASE_ASSERT(canvas);
    RELEASE_ASSERT(path);

    switch (canvas->type) {
        case CANVAS_TYPE_RASTER: {
            raster_canvas_push_clip_path(
                canvas->data.raster,
                path,
                even_odd_rule
            );
            break;
        }
        case CANVAS_TYPE_SCALABLE: {
            scalable_canvas_push_clip_path(
                canvas->data.scalable,
                path,
                even_odd_rule
            );
            break;
        }
        default: {
            LOG_PANIC("Unreachable");
        }
    }
}

void canvas_pop_clip_paths(Canvas* canvas, size_t count) {
    RELEASE_ASSERT(canvas);

    switch (canvas->type) {
        case CANVAS_TYPE_RASTER: {
            raster_canvas_pop_clip_paths(canvas->data.raster, count);
            break;
        }
        case CANVAS_TYPE_SCALABLE: {
            scalable_canvas_pop_clip_paths(canvas->data.scalable, count);
            break;
        }
        default: {
            LOG_PANIC("Unreachable");
        }
    }
}

void canvas_draw_pixel(Canvas* canvas, GeomVec2 position, Rgba rgba) {
    RELEASE_ASSERT(canvas);

    switch (canvas->type) {
        case CANVAS_TYPE_RASTER: {
            LOG_TODO();
        }
        case CANVAS_TYPE_SCALABLE: {
            scalable_canvas_draw_pixel(canvas->data.scalable, position, rgba);
            break;
        }
        default: {
            LOG_PANIC("Unreachable");
        }
    }
}

bool canvas_write_file(Canvas* canvas, const char* path) {
    switch (canvas->type) {
        case CANVAS_TYPE_RASTER: {
            return raster_canvas_write_file(canvas->data.raster, path);
        }
        case CANVAS_TYPE_SCALABLE: {
            return scalable_canvas_write_file(canvas->data.scalable, path);
        }
        default: {
            LOG_PANIC("Unreachable");
        }
    }
}
