#include "dcel.h"

#include <math.h>
#include <stdint.h>
#include <unistd.h>

#include "arena/arena.h"
#include "canvas/canvas.h"
#include "logger/log.h"

#define DVEC_NAME DcelVertices
#define DVEC_LOWERCASE_NAME dcel_vertices
#define DVEC_TYPE DcelVertex
#include "arena/dvec_impl.h"

#define DVEC_NAME DcelHalfEdges
#define DVEC_LOWERCASE_NAME dcel_half_edges
#define DVEC_TYPE DcelHalfEdge
#include "arena/dvec_impl.h"

#define DLINKED_NAME DcelEventQueue
#define DLINKED_LOWERCASE_NAME dcel_event_queue
#define DLINKED_TYPE DcelVertex*
#include "arena/dlinked_impl.h"

#define DLINKED_NAME DcelActiveEdges
#define DLINKED_LOWERCASE_NAME dcel_active_edges
#define DLINKED_TYPE DcelHalfEdge*
#include "arena/dlinked_impl.h"

typedef struct {
    double x;
    double y;
} Point;

#define DVEC_NAME PointVec
#define DVEC_LOWERCASE_NAME point_vec
#define DVEC_TYPE Point
#include "arena/dvec_impl.h"

Dcel* dcel_new(Arena* arena) {
    RELEASE_ASSERT(arena);

    Dcel* dcel = arena_alloc(arena, sizeof(Dcel));
    dcel->arena = arena;
    dcel->vertices = dcel_vertices_new(arena);
    dcel->half_edges = dcel_half_edges_new(arena);
    dcel->event_queue = dcel_event_queue_new(arena);

    return dcel;
}

static bool priority_queue_cmp(DcelVertex** lhs, DcelVertex** rhs) {
    if ((*lhs)->y == (*rhs)->y) {
        return (*lhs)->x < (*rhs)->x;
    }

    return (*lhs)->y < (*rhs)->y;
}

static bool active_edges_cmp(DcelHalfEdge** lhs, DcelHalfEdge** rhs) {
    if ((*lhs)->origin == (*rhs)->origin) {
        return (*lhs)->twin->origin->x < (*rhs)->twin->origin->x;
    }

    return (*lhs)->origin->x < (*rhs)->origin->x;
}

DcelVertex* dcel_add_vertex(Dcel* dcel, double x, double y) {
    RELEASE_ASSERT(dcel);

    DcelVertex* vertex = dcel_vertices_push(
        dcel->vertices,
        (DcelVertex) {.x = x, .y = y, .incident_edge = NULL}
    );
    dcel_event_queue_insert_sorted(
        dcel->event_queue,
        vertex,
        priority_queue_cmp,
        true
    );

    return vertex;
}

DcelHalfEdge* dcel_add_edge(Dcel* dcel, DcelVertex* a, DcelVertex* b) {
    RELEASE_ASSERT(dcel);
    RELEASE_ASSERT(a);
    RELEASE_ASSERT(b);

    DcelHalfEdge* half_edge_a = dcel_half_edges_push(
        dcel->half_edges,
        (DcelHalfEdge) {.origin = a, .twin = NULL, .next = NULL, .prev = NULL}
    );

    DcelHalfEdge* half_edge_b = dcel_half_edges_push(
        dcel->half_edges,
        (DcelHalfEdge) {.origin = b, .twin = NULL, .next = NULL, .prev = NULL}
    );

    half_edge_a->twin = half_edge_b;
    half_edge_b->twin = half_edge_a;

    if (!a->incident_edge) {
        a->incident_edge = half_edge_a;
    }

    if (!b->incident_edge) {
        b->incident_edge = half_edge_b;
    }

    return half_edge_a;
}

DcelHalfEdge* dcel_next_incident_edge(DcelHalfEdge* half_edge) {
    RELEASE_ASSERT(half_edge);

    return half_edge->twin->next;
}

static bool half_edges_share_vertex(DcelHalfEdge* a, DcelHalfEdge* b) {
    RELEASE_ASSERT(a);
    RELEASE_ASSERT(b);

    return a->origin == b->origin || a->twin->origin == b->twin->origin
        || a->twin->origin == b->origin || a->origin == b->twin->origin;
}

static bool compute_intersection_point(
    DcelHalfEdge* a,
    DcelHalfEdge* b,
    double* x_out,
    double* y_out
) {
    double a1_x = a->origin->x;
    double a1_y = a->origin->y;
    double a2_x = a->twin->origin->x;
    double a2_y = a->twin->origin->y;
    double b1_x = b->origin->x;
    double b1_y = b->origin->y;
    double b2_x = b->twin->origin->x;
    double b2_y = b->twin->origin->y;

    double denom =
        (b2_y - b1_y) * (a2_x - a1_x) - (b2_x - b1_x) * (a2_y - a1_y);
    if (fabs(denom) < 1e-9) {
        return false;
    }

    double num1 = (b2_x - b1_x) * (a1_y - b1_y) - (b2_y - b1_y) * (a1_x - b1_x);
    double ua = num1 / denom;
    double num2 = (a2_x - a1_x) * (a1_y - b1_y) - (a2_y - a1_y) * (a1_x - b1_x);
    double ub = num2 / denom;

    if (ua < 0.0 || ua > 1.0 || ub < 0.0 || ub > 1.0) {
        return false;
    }

    double x = a1_x + ua * (a2_x - a1_x);
    double y = a1_y + ua * (a2_y - a1_y);

    *x_out = x;
    *y_out = y;
    return true;
}

static void debug_render(
    Dcel* dcel,
    Dcel* overlay,
    Arena* arena,
    double event_x,
    double event_y,
    DcelActiveEdges* active_edges,
    PointVec* intersections,
    DcelHalfEdge* highlight_a,
    DcelHalfEdge* highlight_b
) {
    RELEASE_ASSERT(dcel);
    RELEASE_ASSERT(overlay);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(active_edges);
    RELEASE_ASSERT(intersections);

    Canvas* canvas = canvas_new(arena, 1000, 900, 0xffffffff);
    dcel_render(dcel, 0x000000ff, canvas);
    dcel_render(overlay, 0x808080ff, canvas);

    for (size_t idx = 0; idx < dcel_active_edges_len(active_edges); idx++) {
        DcelHalfEdge* half_edge;
        RELEASE_ASSERT(dcel_active_edges_get(active_edges, idx, &half_edge));

        canvas_draw_line(
            canvas,
            half_edge->origin->x,
            half_edge->origin->y,
            half_edge->twin->origin->x,
            half_edge->twin->origin->y,
            5.0,
            0x0000ffff
        );
    }

    for (size_t idx = 0; idx < point_vec_len(intersections); idx++) {
        Point point;
        RELEASE_ASSERT(point_vec_get(intersections, idx, &point));

        canvas_draw_circle(canvas, point.x, point.y, 10.0, 0x00ff00ff);
    }

    if (highlight_a) {
        canvas_draw_line(
            canvas,
            highlight_a->origin->x,
            highlight_a->origin->y,
            highlight_a->twin->origin->x,
            highlight_a->twin->origin->y,
            5.0,
            0x00ffffff
        );
    }

    if (highlight_b) {
        canvas_draw_line(
            canvas,
            highlight_b->origin->x,
            highlight_b->origin->y,
            highlight_b->twin->origin->x,
            highlight_b->twin->origin->y,
            5.0,
            0x00ffffff
        );
    }

    canvas_draw_line(
        canvas,
        0.0,
        event_y,
        canvas_width(canvas),
        event_y,
        1.0,
        0xff0000ff
    );
    canvas_draw_circle(canvas, event_x, event_y, 12.0, 0xff0000ff);

    canvas_write_file(canvas, "tessellation.bmp");
    usleep(500000);
}

void dcel_overlay(Dcel* dcel, Dcel* overlay) {
    RELEASE_ASSERT(dcel);
    RELEASE_ASSERT(overlay);
    RELEASE_ASSERT(dcel->arena == overlay->arena);

    dcel_event_queue_merge_sorted(
        dcel->event_queue,
        overlay->event_queue,
        priority_queue_cmp,
        true
    );

    Arena* arena = arena_new(1024);
    DcelEventQueueBlock* event = dcel->event_queue->front;

    DcelActiveEdges* active_edges = dcel_active_edges_new(arena);
    PointVec* intersections = point_vec_new(arena);

    while (event) {
        DcelHalfEdge* incident_edge = event->data->incident_edge;
        do {
            bool removed = false;
            for (size_t idx = 0; idx < dcel_active_edges_len(active_edges);
                 idx++) {
                DcelHalfEdge* half_edge;
                RELEASE_ASSERT(
                    dcel_active_edges_get(active_edges, idx, &half_edge)
                );

                if (incident_edge->twin == half_edge) {
                    DcelHalfEdge* prev_edge = NULL;
                    if (idx > 0) {
                        RELEASE_ASSERT(dcel_active_edges_get(
                            active_edges,
                            idx - 1,
                            &prev_edge
                        ));
                    }

                    DcelHalfEdge* next_edge = NULL;
                    if (idx < dcel_active_edges_len(active_edges) - 1) {
                        RELEASE_ASSERT(dcel_active_edges_get(
                            active_edges,
                            idx + 1,
                            &next_edge
                        ));
                    }

                    if (prev_edge && next_edge
                        && !half_edges_share_vertex(prev_edge, next_edge)) {
                        debug_render(
                            dcel,
                            overlay,
                            arena,
                            event->data->x,
                            event->data->y,
                            active_edges,
                            intersections,
                            prev_edge,
                            next_edge
                        );

                        double intersection_x;
                        double intersection_y;
                        if (compute_intersection_point(
                                prev_edge,
                                next_edge,
                                &intersection_x,
                                &intersection_y
                            )) {
                            point_vec_push(
                                intersections,
                                (Point
                                ) {.x = intersection_x, .y = intersection_y}
                            );
                        }
                    }

                    // Remove edge
                    dcel_active_edges_remove(active_edges, idx);
                    removed = true;
                    break;
                }
            }

            if (!removed) {
                size_t idx = dcel_active_edges_insert_sorted(
                    active_edges,
                    incident_edge,
                    active_edges_cmp,
                    true
                );

                DcelHalfEdge* edge;
                RELEASE_ASSERT(dcel_active_edges_get(active_edges, idx, &edge));

                DcelHalfEdge* prev_edge = NULL;
                if (idx > 0) {
                    RELEASE_ASSERT(
                        dcel_active_edges_get(active_edges, idx - 1, &prev_edge)
                    );
                }

                DcelHalfEdge* next_edge = NULL;
                if (idx < dcel_active_edges_len(active_edges) - 1) {
                    RELEASE_ASSERT(
                        dcel_active_edges_get(active_edges, idx + 1, &next_edge)
                    );
                }

                if (prev_edge && !half_edges_share_vertex(edge, prev_edge)) {
                    debug_render(
                        dcel,
                        overlay,
                        arena,
                        event->data->x,
                        event->data->y,
                        active_edges,
                        intersections,
                        edge,
                        prev_edge
                    );

                    double intersection_x;
                    double intersection_y;
                    if (compute_intersection_point(
                            prev_edge,
                            edge,
                            &intersection_x,
                            &intersection_y
                        )) {
                        point_vec_push(
                            intersections,
                            (Point) {.x = intersection_x, .y = intersection_y}
                        );
                    }
                }

                if (next_edge && !half_edges_share_vertex(edge, next_edge)) {
                    debug_render(
                        dcel,
                        overlay,
                        arena,
                        event->data->x,
                        event->data->y,
                        active_edges,
                        intersections,
                        edge,
                        next_edge
                    );

                    double intersection_x;
                    double intersection_y;
                    if (compute_intersection_point(
                            next_edge,
                            edge,
                            &intersection_x,
                            &intersection_y
                        )) {
                        point_vec_push(
                            intersections,
                            (Point) {.x = intersection_x, .y = intersection_y}
                        );
                    }
                }
            }

            incident_edge = dcel_next_incident_edge(incident_edge);
        } while (incident_edge && incident_edge != event->data->incident_edge);

        debug_render(
            dcel,
            overlay,
            arena,
            event->data->x,
            event->data->y,
            active_edges,
            intersections,
            NULL,
            NULL
        );
        event = event->next;
    }

    arena_free(arena);
}

void dcel_render(Dcel* dcel, uint32_t rgba, Canvas* canvas) {
    RELEASE_ASSERT(dcel);
    RELEASE_ASSERT(canvas);

    for (size_t edge_idx = 0; edge_idx < dcel_half_edges_len(dcel->half_edges);
         edge_idx++) {
        DcelHalfEdge half_edge;
        RELEASE_ASSERT(
            dcel_half_edges_get(dcel->half_edges, edge_idx, &half_edge)
        );

        if (half_edge.twin < half_edge.twin->twin) {
            continue;
        }

        canvas_draw_line(
            canvas,
            half_edge.origin->x,
            half_edge.origin->y,
            half_edge.twin->origin->x,
            half_edge.twin->origin->y,
            2.0,
            rgba
        );
    }

    for (size_t vertex_idx = 0; vertex_idx < dcel_vertices_len(dcel->vertices);
         vertex_idx++) {
        DcelVertex vertex;
        RELEASE_ASSERT(dcel_vertices_get(dcel->vertices, vertex_idx, &vertex));

        canvas_draw_circle(canvas, vertex.x, vertex.y, 10.0, rgba);
    }
}
