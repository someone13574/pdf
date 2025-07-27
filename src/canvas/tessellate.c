#include "tessellate.h"

#include "arena/arena.h"
#include "canvas/canvas.h"
#include "logger/log.h"

#define DVEC_NAME TessPointVec
#define DVEC_LOWERCASE_NAME tess_point_vec
#define DVEC_TYPE TessPoint
#include "arena/dvec_impl.h"

#define DLINKED_NAME TessPointQueue
#define DLINKED_LOWERCASE_NAME tess_point_queue
#define DLINKED_TYPE TessQueuePoint
#include "arena/dlinked_impl.h"

static TessContour tess_contour_new(Arena* arena) {
    RELEASE_ASSERT(arena);

    return (TessContour) {.points = tess_point_vec_new(arena)};
}

static bool tess_compare_points(TessQueuePoint* lhs, TessQueuePoint* rhs) {
    if (lhs->y == rhs->y) {
        return lhs->x < rhs->x;
    }

    return lhs->y < rhs->y;
}

static void tess_contour_add_point(
    TessContour* contour,
    TessPointQueue* point_queue,
    size_t contour_idx,
    double x,
    double y
) {
    RELEASE_ASSERT(contour);

    size_t point_idx = tess_point_vec_len(contour->points);
    tess_point_vec_push(contour->points, (TessPoint) {.x = x, .y = y});
    tess_point_queue_insert_sorted(
        point_queue,
        (TessQueuePoint
        ) {.x = x, .y = y, .point_idx = point_idx, .contour_idx = contour_idx},
        tess_compare_points,
        true
    );
}

#define DVEC_NAME TessContourVec
#define DVEC_LOWERCASE_NAME tess_contour_vec
#define DVEC_TYPE TessContour
#include "arena/dvec_impl.h"

void tess_poly_new(Arena* arena, TessPoly* poly) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(poly);

    poly->arena = arena;
    poly->contours = tess_contour_vec_new(arena);
    poly->point_queue = tess_point_queue_new(arena);
}

void tess_poly_move_to(TessPoly* poly, double x, double y) {
    RELEASE_ASSERT(poly);

    size_t contour_idx = tess_contour_vec_len(poly->contours);
    TessContour contour = tess_contour_new(poly->arena);
    tess_contour_add_point(&contour, poly->point_queue, contour_idx, x, y);

    tess_contour_vec_push(poly->contours, contour);
}

void tess_poly_line_to(TessPoly* poly, double x, double y) {
    RELEASE_ASSERT(poly);

    size_t contour_idx = tess_contour_vec_len(poly->contours);
    RELEASE_ASSERT(contour_idx != 0);

    TessContour* contour;
    RELEASE_ASSERT(
        tess_contour_vec_get_ptr(poly->contours, contour_idx - 1, &contour)
    );

    tess_contour_add_point(contour, poly->point_queue, contour_idx, x, y);
}

static void
tess_poly_debug_render(TessPoly* poly, double active_x, double active_y) {
    RELEASE_ASSERT(poly);

    Arena* arena = arena_new(1024);
    Canvas* canvas = canvas_new(arena, 1000, 900, 0xffffffff);
    tess_poly_render(poly, canvas);

    canvas_draw_line(
        canvas,
        0.0,
        active_y,
        (double)canvas_width(canvas),
        active_y,
        1.0,
        0xa0a0a0ff
    );
    canvas_draw_circle(canvas, active_x, active_y, 10.0, 0xff0000ff);

    canvas_write_file(canvas, "tessellation-debug.bmp");

    arena_free(arena);
}

void tess_poly_tessellate(TessPoly* poly) {
    RELEASE_ASSERT(poly);

    for (size_t queue_idx = 0;
         queue_idx < tess_point_queue_len(poly->point_queue);
         queue_idx++) {
        TessQueuePoint point;
        RELEASE_ASSERT(
            tess_point_queue_get(poly->point_queue, queue_idx, &point)
        );

        tess_poly_debug_render(poly, point.x, point.y);
    }
}

void tess_poly_render(TessPoly* poly, Canvas* canvas) {
    RELEASE_ASSERT(poly);
    RELEASE_ASSERT(canvas);

    for (size_t contour_idx = 0;
         contour_idx < tess_contour_vec_len(poly->contours);
         contour_idx++) {
        TessContour contour;
        RELEASE_ASSERT(
            tess_contour_vec_get(poly->contours, contour_idx, &contour)
        );

        double prev_x = 0.0, prev_y = 0.0, start_x = 0.0, start_y = 0.0;

        for (size_t point_idx = 0;
             point_idx < tess_point_vec_len(contour.points);
             point_idx++) {
            TessPoint point;
            RELEASE_ASSERT(tess_point_vec_get(contour.points, point_idx, &point)
            );

            if (point_idx != 0) {
                canvas_draw_line(
                    canvas,
                    point.x,
                    point.y,
                    prev_x,
                    prev_y,
                    2.0,
                    0x000000ff
                );
            } else {
                start_x = point.x;
                start_y = point.y;
            }

            if (point_idx + 1 == tess_point_vec_len(contour.points)) {
                canvas_draw_line(
                    canvas,
                    start_x,
                    start_y,
                    point.x,
                    point.y,
                    2.0,
                    0x000000ff
                );
            }

            prev_x = point.x;
            prev_y = point.y;
        }
    }
}
