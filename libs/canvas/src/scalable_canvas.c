#include "scalable_canvas.h"

#include <stdio.h>
#include <string.h>

#include "arena/arena.h"
#include "canvas/canvas.h"
#include "logger/log.h"
#include "path_builder.h"
#include "str/alloc_str.h"

#define DVEC_NAME SvgPartsVec
#define DVEC_LOWERCASE_NAME svg_parts_vec
#define DVEC_TYPE Str*
#include "arena/dvec_impl.h"

struct ScalableCanvas {
    Arena* arena;

    uint32_t width;
    uint32_t height;
    double raster_res;

    SvgPartsVec* parts;
};

ScalableCanvas* scalable_canvas_new(
    Arena* arena,
    uint32_t width,
    uint32_t height,
    uint32_t rgba,
    double raster_res
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(raster_res > 1e-3);

    ScalableCanvas* canvas = arena_alloc(arena, sizeof(ScalableCanvas));
    canvas->arena = arena;
    canvas->width = width;
    canvas->height = height;
    canvas->raster_res = raster_res;
    canvas->parts = svg_parts_vec_new(arena);

    svg_parts_vec_push(
        canvas->parts,
        str_new_fmt(
            arena,
            "<rect width=\"%u\" height=\"%u\" fill=\"#%08x\" />",
            width,
            height,
            rgba
        )
    );

    return canvas;
}

double scalable_canvas_raster_res(ScalableCanvas* canvas) {
    RELEASE_ASSERT(canvas);
    return canvas->raster_res;
}

void scalable_canvas_draw_circle(
    ScalableCanvas* canvas,
    double x,
    double y,
    double radius,
    uint32_t rgba
) {
    RELEASE_ASSERT(canvas);

    svg_parts_vec_push(
        canvas->parts,
        str_new_fmt(
            canvas->arena,
            "<circle cx=\"%f\" cy=\"%f\" r=\"%f\" fill=\"#%08x\" />",
            x,
            y,
            radius,
            rgba
        )
    );
}

void scalable_canvas_draw_line(
    ScalableCanvas* canvas,
    double x1,
    double y1,
    double x2,
    double y2,
    double radius,
    uint32_t rgba
) {
    RELEASE_ASSERT(canvas);

    svg_parts_vec_push(
        canvas->parts,
        str_new_fmt(
            canvas->arena,
            "<line x1=\"%f\" y1=\"%f\" x2=\"%f\" y2=\"%f\" stroke=\"#%08x\" stroke-width=\"%f\" fill=\"transparent\" />",
            x1,
            y1,
            x2,
            y2,
            rgba,
            radius
        )
    );
}

void scalable_canvas_draw_bezier(
    ScalableCanvas* canvas,
    double x1,
    double y1,
    double x2,
    double y2,
    double cx,
    double cy,
    double radius,
    uint32_t rgba
) {
    RELEASE_ASSERT(canvas);

    svg_parts_vec_push(
        canvas->parts,
        str_new_fmt(
            canvas->arena,
            "<path d=\"M %f %f Q %f %f %f %f\" stroke=\"#%08x\" stroke-width=\"%f\" fill=\"transparent\" />",
            x1,
            y1,
            cx,
            cy,
            x2,
            y2,
            rgba,
            radius
        )
    );
}

void scalable_canvas_draw_path(
    ScalableCanvas* canvas,
    const PathBuilder* path,
    CanvasBrush brush
) {
    RELEASE_ASSERT(canvas);
    RELEASE_ASSERT(path);

    svg_parts_vec_push(canvas->parts, str_new_fmt(canvas->arena, "<path d=\""));

    for (size_t contour_idx = 0;
         contour_idx < path_contour_vec_len(path->contours);
         contour_idx++) {
        PathContour* contour = NULL;
        RELEASE_ASSERT(
            path_contour_vec_get(path->contours, contour_idx, &contour)
        );

        if (contour_idx != 0) {
            svg_parts_vec_push(canvas->parts, str_new_fmt(canvas->arena, "Z "));
        }

        for (size_t segment_idx = 0; segment_idx < path_contour_len(contour);
             segment_idx++) {
            PathContourSegment segment;
            RELEASE_ASSERT(path_contour_get(contour, segment_idx, &segment));

            switch (segment.type) {
                case PATH_CONTOUR_SEGMENT_TYPE_START: {
                    svg_parts_vec_push(
                        canvas->parts,
                        str_new_fmt(
                            canvas->arena,
                            "M %f %f ",
                            segment.value.start.x,
                            segment.value.start.y
                        )
                    );
                    break;
                }
                case PATH_CONTOUR_SEGMENT_TYPE_LINE: {
                    svg_parts_vec_push(
                        canvas->parts,
                        str_new_fmt(
                            canvas->arena,
                            "L %f %f ",
                            segment.value.line.x,
                            segment.value.line.y
                        )
                    );
                    break;
                }
                case PATH_CONTOUR_SEGMENT_TYPE_QUAD_BEZIER: {
                    svg_parts_vec_push(
                        canvas->parts,
                        str_new_fmt(
                            canvas->arena,
                            "Q %f %f %f %f ",
                            segment.value.quad_bezier.control.x,
                            segment.value.quad_bezier.control.y,
                            segment.value.quad_bezier.end.x,
                            segment.value.quad_bezier.end.y
                        )
                    );
                    break;
                }
                case PATH_CONTOUR_SEGMENT_TYPE_CUBIC_BEZIER: {
                    svg_parts_vec_push(
                        canvas->parts,
                        str_new_fmt(
                            canvas->arena,
                            "C %f %f, %f %f, %f %f ",
                            segment.value.cubic_bezier.control_a.x,
                            segment.value.cubic_bezier.control_a.y,
                            segment.value.cubic_bezier.control_b.x,
                            segment.value.cubic_bezier.control_b.y,
                            segment.value.cubic_bezier.end.x,
                            segment.value.cubic_bezier.end.y
                        )
                    );
                    break;
                }
            }
        }
    }

    // Brush
    if (brush.enable_fill) {
        svg_parts_vec_push(
            canvas->parts,
            str_new_fmt(canvas->arena, "\" fill=\"#%08x\"", brush.fill_rgba)
        );
    } else {
        svg_parts_vec_push(
            canvas->parts,
            str_new_fmt(canvas->arena, "\" fill=\"none\"")
        );
    }

    if (brush.enable_stroke) {
        svg_parts_vec_push(
            canvas->parts,
            str_new_fmt(
                canvas->arena,
                " stroke=\"#%08x\" stroke-width=\"%f\"",
                brush.stroke_rgba,
                brush.stroke_width
            )
        );

        const char* line_cap = NULL;
        switch (brush.line_cap) {
            case CANVAS_LINECAP_BUTT: {
                line_cap = "butt";
                break;
            }
            case CANVAS_LINECAP_ROUND: {
                line_cap = "round";
                break;
            }
            case CANVAS_LINECAP_SQUARE: {
                line_cap = "square";
                break;
            }
        }
        svg_parts_vec_push(
            canvas->parts,
            str_new_fmt(canvas->arena, " stroke-linecap=\"%s\"", line_cap)
        );

        const char* line_join = NULL;
        switch (brush.line_join) {
            case CANVAS_LINEJOIN_MITER: {
                line_join = "miter";
                break;
            }
            case CANVAS_LINEJOIN_ROUND: {
                line_join = "round";
                break;
            }
            case CANVAS_LINEJOIN_BEVEL: {
                line_join = "bevel";
                break;
            }
        }
        svg_parts_vec_push(
            canvas->parts,
            str_new_fmt(canvas->arena, " stroke-linejoin=\"%s\"", line_join)
        );

        if (brush.line_join == CANVAS_LINEJOIN_MITER) {
            svg_parts_vec_push(
                canvas->parts,
                str_new_fmt(
                    canvas->arena,
                    " stroke-miterlimit=\"%f\"",
                    brush.miter_limit
                )
            );
        }
    }

    svg_parts_vec_push(canvas->parts, str_new_fmt(canvas->arena, "  />"));
}

void scalable_canvas_draw_pixel(
    ScalableCanvas* canvas,
    GeomVec2 position,
    uint32_t rgba
) {
    RELEASE_ASSERT(canvas);

    svg_parts_vec_push(
        canvas->parts,
        str_new_fmt(
            canvas->arena,
            "<rect x=\"%f\" y=\"%f\" width=\"%f\" height=\"%f\" fill=\"#%08x\" />",
            position.x,
            position.y,
            canvas->raster_res,
            canvas->raster_res,
            rgba
        )
    );
}

bool scalable_canvas_write_file(ScalableCanvas* canvas, const char* path) {
    FILE* file = fopen(path, "w");
    if (!file) {
        return false;
    }

    if (fprintf(
            file,
            "<svg version=\"1.1\" width=\"%zu\" height=\"%zu\" xmlns=\"http://www.w3.org/2000/svg\">",
            (size_t)canvas->width,
            (size_t)canvas->height
        )
        < 0) {
        fclose(file);
        return false;
    }

    for (size_t idx = 0; idx < svg_parts_vec_len(canvas->parts); ++idx) {
        Str* operation;
        RELEASE_ASSERT(svg_parts_vec_get(canvas->parts, idx, &operation));

        const char* opbuf = str_get_cstr(operation);
        size_t oplen = strlen(opbuf);

        if (oplen > 0) {
            size_t written = fwrite(opbuf, 1, oplen, file);
            if (written != oplen) {
                fclose(file);
                return false;
            }
        }
    }

    if (fputs("</svg>", file) == EOF) {
        fclose(file);
        return false;
    }

    if (fclose(file) != 0) {
        return false;
    }

    return true;
}
