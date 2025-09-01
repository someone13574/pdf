#include "scalable_canvas.h"

#include <stdio.h>
#include <string.h>

#include "arena/arena.h"
#include "arena/string.h"
#include "logger/log.h"
#include "path_builder.h"

#define DVEC_NAME SvgPartsVec
#define DVEC_LOWERCASE_NAME svg_parts_vec
#define DVEC_TYPE ArenaString*
#include "arena/dvec_impl.h"

struct ScalableCanvas {
    Arena* arena;

    uint32_t width;
    uint32_t height;

    SvgPartsVec* parts;
};

ScalableCanvas*
scalable_canvas_new(Arena* arena, uint32_t width, uint32_t height) {
    RELEASE_ASSERT(arena);

    ScalableCanvas* canvas = arena_alloc(arena, sizeof(ScalableCanvas));
    canvas->arena = arena;
    canvas->width = width;
    canvas->height = height;
    canvas->parts = svg_parts_vec_new(arena);

    return canvas;
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
        arena_string_new_fmt(
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
        arena_string_new_fmt(
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
        arena_string_new_fmt(
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

void scalable_canvas_draw_path(ScalableCanvas* canvas, PathBuilder* path) {
    RELEASE_ASSERT(canvas);
    RELEASE_ASSERT(path);

    svg_parts_vec_push(
        canvas->parts,
        arena_string_new_fmt(canvas->arena, "<path d=\"")
    );

    for (size_t contour_idx = 0;
         contour_idx < path_contour_vec_len(path->contours);
         contour_idx++) {
        PathContour* contour = NULL;
        RELEASE_ASSERT(
            path_contour_vec_get(path->contours, contour_idx, &contour)
        );

        if (contour_idx != 0) {
            svg_parts_vec_push(
                canvas->parts,
                arena_string_new_fmt(canvas->arena, "Z ")
            );
        }

        for (size_t segment_idx = 0; segment_idx < path_contour_len(contour);
             segment_idx++) {
            PathContourSegment segment;
            RELEASE_ASSERT(path_contour_get(contour, segment_idx, &segment));

            switch (segment.type) {
                case PATH_CONTOUR_SEGMENT_TYPE_START: {
                    svg_parts_vec_push(
                        canvas->parts,
                        arena_string_new_fmt(
                            canvas->arena,
                            "M %f %f ",
                            segment.data.start.x,
                            segment.data.start.y
                        )
                    );
                    break;
                }
                case PATH_CONTOUR_SEGMENT_TYPE_LINE: {
                    svg_parts_vec_push(
                        canvas->parts,
                        arena_string_new_fmt(
                            canvas->arena,
                            "L %f %f ",
                            segment.data.line.x,
                            segment.data.line.y
                        )
                    );
                    break;
                }
                case PATH_CONTOUR_SEGMENT_TYPE_BEZIER: {
                    svg_parts_vec_push(
                        canvas->parts,
                        arena_string_new_fmt(
                            canvas->arena,
                            "Q %f %f %f %f ",
                            segment.data.bezier.control.x,
                            segment.data.bezier.control.y,
                            segment.data.bezier.end.x,
                            segment.data.bezier.end.y
                        )
                    );
                    break;
                }
            }
        }
    }

    svg_parts_vec_push(
        canvas->parts,
        arena_string_new_fmt(canvas->arena, "\" fill=\"black\" />")
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
        ArenaString* operation;
        RELEASE_ASSERT(svg_parts_vec_get(canvas->parts, idx, &operation));

        const char* opbuf = arena_string_buffer(operation);
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
