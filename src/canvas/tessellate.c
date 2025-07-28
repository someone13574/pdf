#include "tessellate.h"

#include "arena/arena.h"
#include "canvas/canvas.h"
#include "logger/log.h"

#define DLINKED_NAME TessPointQueue
#define DLINKED_LOWERCASE_NAME tess_point_queue
#define DLINKED_TYPE TessPoint*
#include "arena/dlinked_impl.h"

typedef struct {
    TessPoint* helper;

    TessPoint* start;
    TessPoint* end;
} TessActiveEdge;

#define DLINKED_NAME TessActiveEdges
#define DLINKED_LOWERCASE_NAME tess_active_edges
#define DLINKED_TYPE TessActiveEdge
#include "arena/dlinked_impl.h"

static TessContour tess_contour_new(Arena* arena) {
    RELEASE_ASSERT(arena);

    return (TessContour) {.start = NULL, .end = NULL};
}

static TessPoint* tess_contour_split(
    Arena* arena,
    TessPoint* point_a,
    TessPoint* point_b,
    TessContourVec* contours
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(point_a);
    RELEASE_ASSERT(point_b);
    RELEASE_ASSERT(contours);
    RELEASE_ASSERT(point_a->contour == point_b->contour);

    // Make a copy of the splice points
    TessPoint* point_a2 = arena_alloc(arena, sizeof(TessPoint));
    TessPoint* point_b2 = arena_alloc(arena, sizeof(TessPoint));
    *point_a2 = *point_a;
    *point_b2 = *point_b;

    // Close the original contour
    point_a->next = point_b;
    point_b->prev = point_a;
    point_a->contour->start = point_b;
    point_a->contour->end = point_a;

    // Create a new contour
    TessContour* new_contour = tess_contour_vec_push(
        contours,
        (TessContour) {.start = point_a2, .end = point_b2}
    );

    // Close the new contour
    point_a2->prev = point_b2;
    point_b2->next = point_a2;
    point_a2->next->prev = point_a2;
    point_b2->prev->next = point_b2;

    // Assign moved points to new contour
    TessPoint* point = point_a2;
    do {
        point->contour = new_contour;
        point = point->next;
    } while (point != point_a2);

    return point_a2;
}

static bool
tess_compare_xy(double lhs_x, double lhs_y, double rhs_x, double rhs_y) {
    if (lhs_y == rhs_y) {
        return lhs_x < rhs_x;
    }

    return lhs_y < rhs_y;
}

static bool tess_compare_points(TessPoint** lhs, TessPoint** rhs) {
    RELEASE_ASSERT(lhs);
    RELEASE_ASSERT(rhs);
    RELEASE_ASSERT(*lhs);
    RELEASE_ASSERT(*rhs);

    return tess_compare_xy((*lhs)->x, (*lhs)->y, (*rhs)->x, (*rhs)->y);
}

static void tess_contour_add_point(
    Arena* arena,
    TessContour* contour,
    TessPointQueue* point_queue,
    double x,
    double y
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(contour);
    RELEASE_ASSERT(point_queue);

    TessPoint* point = arena_alloc(arena, sizeof(TessPoint));
    point->x = x;
    point->y = y;
    point->type = TESS_POINT_TYPE_REGULAR;
    point->prev_below = false;
    point->next_below = false;
    point->contour = contour;

    if (!contour->start) {
        point->prev = point;
        point->next = point;
        contour->start = point;
        contour->end = point;
    } else {
        point->prev = contour->end;
        point->next = contour->start;
        contour->end->next = point;
        contour->start->prev = point;
        contour->end = point;
    }

    tess_point_queue_insert_sorted(
        point_queue,
        point,
        tess_compare_points,
        true
    );
}

static void tess_contour_assign_types(TessContour* contour) {
    RELEASE_ASSERT(contour);

    TessPoint* point = contour->start;
    if (!point) {
        return;
    }

    do {
        point->prev_below =
            tess_compare_xy(point->x, point->y, point->prev->x, point->prev->y);
        point->next_below =
            tess_compare_xy(point->x, point->y, point->next->x, point->next->y);
        bool left_to_right =
            point->prev->x < point->x && point->x < point->next->x;

        if (point->prev_below && point->next_below) {
            if (left_to_right) {
                point->type = TESS_POINT_TYPE_START;
            } else {
                point->type = TESS_POINT_TYPE_SPLIT;
            }
        } else if (!point->prev_below && !point->next_below) {
            if (left_to_right) {
                point->type = TESS_POINT_TYPE_MERGE;
            } else {
                point->type = TESS_POINT_TYPE_END;
            }
        }

        point = point->next;
    } while (point != contour->start);
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

    TessContour contour = tess_contour_new(poly->arena);
    tess_contour_add_point(poly->arena, &contour, poly->point_queue, x, y);

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

    tess_contour_add_point(poly->arena, contour, poly->point_queue, x, y);
}

static void tess_poly_debug_render(
    TessPoly* poly,
    TessActiveEdges* edges,
    double active_x,
    double active_y,
    size_t highlight_edge
) {
    RELEASE_ASSERT(poly);
    RELEASE_ASSERT(edges);

    Arena* arena = arena_new(1024);
    Canvas* canvas = canvas_new(arena, 1000, 900, 0xffffffff);
    tess_poly_render(poly, canvas);

    for (size_t edge_idx = 0; edge_idx < tess_active_edges_len(edges);
         edge_idx++) {
        TessActiveEdge edge;
        RELEASE_ASSERT(tess_active_edges_get(edges, edge_idx, &edge));

        canvas_draw_line(
            canvas,
            edge.start->x,
            edge.start->y,
            edge.end->x,
            edge.end->y,
            highlight_edge == edge_idx ? 6.0 : 3.0,
            0xfcba03ff
        );
    }

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

    Arena* arena = arena_new(1024);
    TessActiveEdges* active_edges = tess_active_edges_new(arena);

    for (size_t contour_idx = 0;
         contour_idx < tess_contour_vec_len(poly->contours);
         contour_idx++) {
        TessContour* contour;
        RELEASE_ASSERT(
            tess_contour_vec_get_ptr(poly->contours, contour_idx, &contour)
        );
        tess_contour_assign_types(contour);
    }

    for (size_t queue_idx = 0;
         queue_idx < tess_point_queue_len(poly->point_queue);
         queue_idx++) {
        TessPoint* point;
        RELEASE_ASSERT(
            tess_point_queue_get(poly->point_queue, queue_idx, &point)
        );

        // Remove terminated edges and become helper
        double project_x = -1.0;
        TessActiveEdge* project_edge = NULL;
        size_t project_edge_idx = 100;

        for (ssize_t edge_idx =
                 (ssize_t)tess_active_edges_len(active_edges) - 1;
             edge_idx >= 0;
             edge_idx--) {
            TessActiveEdge* edge;
            RELEASE_ASSERT(
                tess_active_edges_get_ptr(active_edges, (size_t)edge_idx, &edge)
            );

            if (edge->end == point) {
                continue;
            }

            // Find intersection
            if ((edge->start->y < point->y && point->y < edge->end->y)
                || (edge->end->y < point->y && point->y < edge->start->y)) {
                double intersection_x = edge->start->x
                    + (edge->end->x - edge->start->x)
                        * (point->y - edge->start->y)
                        / (edge->end->y - edge->start->y);
                if (intersection_x < point->x && intersection_x > project_x) {
                    project_x = intersection_x;
                    project_edge = edge;
                }
            }
        }

        if (project_edge) {
            if (point->type == TESS_POINT_TYPE_SPLIT
                || project_edge->helper->type == TESS_POINT_TYPE_MERGE) {
                point = tess_contour_split(
                    poly->arena,
                    point,
                    project_edge->helper,
                    poly->contours
                );
            }

            project_edge->helper = point;
        }

        tess_poly_debug_render(
            poly,
            active_edges,
            point->x,
            point->y,
            project_edge_idx
        );

        // Add edges
        if (point->prev_below) {
            tess_active_edges_push_back(
                active_edges,
                (TessActiveEdge
                ) {.helper = point, .start = point, .end = point->prev}
            );
        }

        if (point->next_below) {
            tess_active_edges_push_back(
                active_edges,
                (TessActiveEdge
                ) {.helper = point, .start = point, .end = point->next}
            );
        }

        // Remove terminated edges
        for (ssize_t edge_idx =
                 (ssize_t)tess_active_edges_len(active_edges) - 1;
             edge_idx >= 0;
             edge_idx--) {
            TessActiveEdge* edge;
            RELEASE_ASSERT(
                tess_active_edges_get_ptr(active_edges, (size_t)edge_idx, &edge)
            );

            // Terminate edge
            if (edge->end == point) {
                if (edge->helper->type == TESS_POINT_TYPE_MERGE) {
                    point = tess_contour_split(
                        poly->arena,
                        point,
                        edge->helper,
                        poly->contours
                    );
                    edge->helper = point;
                }

                tess_poly_debug_render(
                    poly,
                    active_edges,
                    point->x,
                    point->y,
                    project_edge_idx
                );

                // tess_active_edges_remove(active_edges, (size_t)edge_idx);
            }
        }
    }

    arena_free(arena);
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

        if (!contour.start) {
            continue;
        }

        TessPoint* point = contour.start;
        do {
            canvas_draw_line(
                canvas,
                point->x,
                point->y,
                point->next->x,
                point->next->y,
                2.0,
                0x000000ff
            );

            point = point->next;
        } while (point != contour.start);
    }
}
