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

#define DVEC_NAME DcelFaces
#define DVEC_LOWERCASE_NAME dcel_faces
#define DVEC_TYPE DcelFace*
#include "arena/dvec_impl.h"

typedef struct {
    DcelHalfEdge* edge;
    DcelVertex* helper;
} ActiveEdge;

#define DLINKED_NAME DcelActiveEdges
#define DLINKED_LOWERCASE_NAME dcel_active_edges
#define DLINKED_TYPE ActiveEdge
#define DLINKED_SORT_USER_DATA double
#include "arena/dlinked_impl.h"

Dcel* dcel_new(Arena* arena) {
    RELEASE_ASSERT(arena);

    Dcel* dcel = arena_alloc(arena, sizeof(Dcel));
    dcel->arena = arena;
    dcel->vertices = dcel_vertices_new(arena);
    dcel->half_edges = dcel_half_edges_new(arena);
    dcel->faces = dcel_faces_new(arena);
    dcel->outer_face = arena_alloc(arena, sizeof(DcelFace));
    dcel->event_queue = dcel_event_queue_new(arena);

    dcel_faces_push(dcel->faces, dcel->outer_face);

    return dcel;
}

static bool priority_queue_cmp(DcelVertex** lhs, DcelVertex** rhs) {
    if ((*lhs)->y == (*rhs)->y) {
        return (*lhs)->x < (*rhs)->x;
    }

    return (*lhs)->y < (*rhs)->y;
}

static double edge_intersect_x(DcelHalfEdge* edge, double sweep_y) {
    double x1 = edge->origin->x;
    double y1 = edge->origin->y;
    double x2 = edge->twin->origin->x;
    double y2 = edge->twin->origin->y;

    double dy = y2 - y1;
    if (fabs(dy) < 1e-12) {
        return fmin(x1, x2);
    }

    double t = (sweep_y - y1) / dy;
    return x1 + t * (x2 - x1);
}

static bool active_edges_cmp(ActiveEdge* lhs, ActiveEdge* rhs, double sweep_y) {
    double ax = edge_intersect_x(lhs->edge, sweep_y);
    double bx = edge_intersect_x(rhs->edge, sweep_y);

    if (fabs(ax - bx) > 1e-5) {
        return ax < bx;
    }

    // Same x — use orientation at a point slightly above sweep line
    ax = edge_intersect_x(lhs->edge, sweep_y + 1e-5);
    bx = edge_intersect_x(rhs->edge, sweep_y + 1e-5);

    return ax < bx;
}

DcelVertex* dcel_add_vertex(Dcel* dcel, double x, double y) {
    RELEASE_ASSERT(dcel);

    DcelVertex* vertex = dcel_vertices_push(
        dcel->vertices,
        (DcelVertex) {.x = x, .y = y, .incident_edge = NULL, .highlight = false}
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
        (DcelHalfEdge
        ) {.origin = a,
           .twin = NULL,
           .next = NULL,
           .prev = NULL,
           .primary = true}
    );

    DcelHalfEdge* half_edge_b = dcel_half_edges_push(
        dcel->half_edges,
        (DcelHalfEdge
        ) {.origin = b,
           .twin = NULL,
           .next = NULL,
           .prev = NULL,
           .primary = false}
    );

    half_edge_a->twin = half_edge_b;
    half_edge_b->twin = half_edge_a;

    a->incident_edge = half_edge_a;
    b->incident_edge = half_edge_b;

    return half_edge_a;
}

static DcelHalfEdge*
split_edge_at_point(Dcel* dcel, DcelHalfEdge* half_edge, DcelVertex* vertex) {
    RELEASE_ASSERT(dcel);
    RELEASE_ASSERT(half_edge);
    RELEASE_ASSERT(half_edge->twin);
    RELEASE_ASSERT(vertex);

    DcelHalfEdge* new_half_edge =
        dcel_add_edge(dcel, vertex, half_edge->twin->origin);
    half_edge->twin->origin = vertex;

    new_half_edge->next = half_edge->next;
    half_edge->next->prev = new_half_edge;
    half_edge->twin->prev->next = new_half_edge->twin;
    new_half_edge->twin->prev = half_edge->twin->prev;

    half_edge->next = new_half_edge;
    new_half_edge->prev = half_edge;
    new_half_edge->twin->next = half_edge->twin;
    half_edge->twin->prev = new_half_edge->twin;

    return new_half_edge;
}

typedef struct {
    DcelHalfEdge* half_edge;
    double angle;
} IncidentAngle;

#define DLINKED_NAME IncidentAnglesList
#define DLINKED_LOWERCASE_NAME incident_angles_list
#define DLINKED_TYPE IncidentAngle
#include "arena/dlinked_impl.h"

static bool cmp_incident_angles_list(IncidentAngle* lhs, IncidentAngle* rhs) {
    RELEASE_ASSERT(lhs);
    RELEASE_ASSERT(rhs);

    return lhs->angle < rhs->angle;
}

DcelVertex* dcel_intersect_edges(
    Dcel* dcel,
    DcelHalfEdge* a,
    DcelHalfEdge* b,
    double intersection_x,
    double intersection_y
) {
    LOG_DIAG(
        INFO,
        DCEL,
        "Intersection at %.1f,%.1f",
        intersection_x,
        intersection_y
    );

    RELEASE_ASSERT(dcel);
    RELEASE_ASSERT(a);
    RELEASE_ASSERT(b);

    DcelVertex* vertex = dcel_add_vertex(dcel, intersection_x, intersection_y);
    DcelHalfEdge* a_prime = split_edge_at_point(dcel, a, vertex);
    DcelHalfEdge* b_prime = split_edge_at_point(dcel, b, vertex);
    vertex->incident_edge = a_prime;

    Arena* local_arena = arena_new(128);
    IncidentAnglesList* incident_angles = incident_angles_list_new(local_arena);

    DcelHalfEdge* incident_edges[4] = {a_prime, b_prime, a->twin, b->twin};
    for (size_t idx = 0; idx < 4; idx++) {
        double delta_x = incident_edges[idx]->twin->origin->x - vertex->x;
        double delta_y = incident_edges[idx]->twin->origin->y - vertex->y;
        double angle = atan2(delta_y, delta_x);

        incident_angles_list_insert_sorted(
            incident_angles,
            (IncidentAngle) {.half_edge = incident_edges[idx], .angle = angle},
            cmp_incident_angles_list,
            true
        );
    }

    for (size_t idx_a = 0; idx_a < incident_angles_list_len(incident_angles);
         ++idx_a) {
        size_t idx_b = (idx_a + 1) % incident_angles_list_len(incident_angles);
        IncidentAngle edge_a;
        IncidentAngle edge_b;
        RELEASE_ASSERT(incident_angles_list_get(incident_angles, idx_a, &edge_a)
        );
        RELEASE_ASSERT(incident_angles_list_get(incident_angles, idx_b, &edge_b)
        );

        edge_a.half_edge->twin->next = edge_b.half_edge;
        edge_b.half_edge->prev = edge_a.half_edge->twin;
    }

    arena_free(local_arena);
    return vertex;
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
    Arena* arena,
    double event_x,
    double event_y,
    DcelActiveEdges* active_edges,
    DcelHalfEdge* highlight_edge,
    DcelHalfEdge* highlight_a,
    DcelHalfEdge* highlight_b
) {
    RELEASE_ASSERT(dcel);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(active_edges);

    Canvas* canvas = canvas_new(arena, 2000, 1800, 0xffffffff, 2.0);
    for (size_t idx = 0; idx < dcel_active_edges_len(active_edges); idx++) {
        ActiveEdge active_edge;
        RELEASE_ASSERT(dcel_active_edges_get(active_edges, idx, &active_edge));

        canvas_draw_line(
            canvas,
            active_edge.edge->origin->x,
            active_edge.edge->origin->y,
            active_edge.edge->twin->origin->x,
            active_edge.edge->twin->origin->y,
            10.0,
            0x0000ffff
        );
    }

    dcel_render(dcel, 0x000000ff, canvas, highlight_edge);

    for (size_t idx = 0; idx < dcel_active_edges_len(active_edges); idx++) {
        ActiveEdge active_edge;
        RELEASE_ASSERT(dcel_active_edges_get(active_edges, idx, &active_edge));

        if (!active_edge.helper) {
            continue;
        }

        if (fabs(active_edge.edge->origin->x - active_edge.helper->x) < 1e-6) {
            canvas_draw_line(
                canvas,
                active_edge.helper->x,
                active_edge.helper->y,
                active_edge.edge->origin->x * 0.5
                    + active_edge.edge->twin->origin->x * 0.5,
                active_edge.edge->origin->y * 0.5
                    + active_edge.edge->twin->origin->y * 0.5,
                5.0,
                0x00ff00ff
            );
        } else {
            canvas_draw_line(
                canvas,
                edge_intersect_x(active_edge.edge, active_edge.helper->y),
                active_edge.helper->y,
                active_edge.helper->x,
                active_edge.helper->y,
                5.0,
                0x00ff00ff
            );
        }
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
    usleep(100000);
}

void dcel_overlay(Dcel* dcel) {
    RELEASE_ASSERT(dcel);

    Arena* local_arena = arena_new(1024);
    DcelActiveEdges* active_edges = dcel_active_edges_new(local_arena);

    DcelEventQueueBlock* event = dcel->event_queue->front;
    while (event) {
        DcelHalfEdge* incident_edge = event->data->incident_edge;
        do {
            bool removed = false;
            for (size_t idx = 0; idx < dcel_active_edges_len(active_edges);
                 idx++) {
                ActiveEdge active_edge;
                RELEASE_ASSERT(
                    dcel_active_edges_get(active_edges, idx, &active_edge)
                );

                if (incident_edge->twin == active_edge.edge) {
                    ActiveEdge* prev_edge = NULL;
                    if (idx > 0) {
                        RELEASE_ASSERT(dcel_active_edges_get_ptr(
                            active_edges,
                            idx - 1,
                            &prev_edge
                        ));
                    }

                    ActiveEdge* next_edge = NULL;
                    if (idx < dcel_active_edges_len(active_edges) - 1) {
                        RELEASE_ASSERT(dcel_active_edges_get_ptr(
                            active_edges,
                            idx + 1,
                            &next_edge
                        ));
                    }

                    if (prev_edge && next_edge
                        && !half_edges_share_vertex(
                            prev_edge->edge,
                            next_edge->edge
                        )) {
                        LOG_DIAG(
                            TRACE,
                            DCEL,
                            "Intersect test: prev (%.1f,%.1f -> %.1f,%.1f, %p) against next (%.1f,%.1f -> %.1f,%.1f, %p)",
                            prev_edge->edge->origin->x,
                            prev_edge->edge->origin->y,
                            prev_edge->edge->twin->origin->x,
                            prev_edge->edge->twin->origin->y,
                            (void*)prev_edge->edge,
                            next_edge->edge->origin->x,
                            next_edge->edge->origin->y,
                            next_edge->edge->twin->origin->x,
                            next_edge->edge->twin->origin->y,
                            (void*)next_edge->edge
                        );

                        double intersection_x;
                        double intersection_y;
                        if (compute_intersection_point(
                                prev_edge->edge,
                                next_edge->edge,
                                &intersection_x,
                                &intersection_y
                            )) {
                            dcel_intersect_edges(
                                dcel,
                                prev_edge->edge,
                                next_edge->edge,
                                intersection_x,
                                intersection_y
                            );
                        }
                    }

                    // Remove edge
                    LOG_DIAG(
                        INFO,
                        DCEL,
                        "Removing edge %p",
                        (void*)active_edge.edge
                    );
                    dcel_active_edges_remove(active_edges, idx);
                    removed = true;
                    break;
                }
            }

            if (!removed) {
                size_t idx = dcel_active_edges_insert_sorted(
                    active_edges,
                    (ActiveEdge) {.edge = incident_edge, .helper = NULL},
                    active_edges_cmp,
                    event->data->y,
                    true
                );

                ActiveEdge edge;
                RELEASE_ASSERT(dcel_active_edges_get(active_edges, idx, &edge));
                LOG_DIAG(INFO, DCEL, "Inserting edge %p", (void*)edge.edge);

                ActiveEdge* prev_edge = NULL;
                if (idx > 0) {
                    RELEASE_ASSERT(dcel_active_edges_get_ptr(
                        active_edges,
                        idx - 1,
                        &prev_edge
                    ));
                }

                ActiveEdge* next_edge = NULL;
                if (idx < dcel_active_edges_len(active_edges) - 1) {
                    RELEASE_ASSERT(dcel_active_edges_get_ptr(
                        active_edges,
                        idx + 1,
                        &next_edge
                    ));
                }

                if (prev_edge
                    && !half_edges_share_vertex(edge.edge, prev_edge->edge)) {
                    LOG_DIAG(
                        TRACE,
                        DCEL,
                        "Intersect test: prev (%.1f,%.1f -> %.1f,%.1f, %p) against inserted (%.1f,%.1f -> %.1f,%.1f, %p)",
                        prev_edge->edge->origin->x,
                        prev_edge->edge->origin->y,
                        prev_edge->edge->twin->origin->x,
                        prev_edge->edge->twin->origin->y,
                        (void*)prev_edge->edge,
                        edge.edge->origin->x,
                        edge.edge->origin->y,
                        edge.edge->twin->origin->x,
                        edge.edge->twin->origin->y,
                        (void*)edge.edge
                    );

                    double intersection_x;
                    double intersection_y;
                    if (compute_intersection_point(
                            prev_edge->edge,
                            edge.edge,
                            &intersection_x,
                            &intersection_y
                        )) {
                        dcel_intersect_edges(
                            dcel,
                            edge.edge,
                            prev_edge->edge,
                            intersection_x,
                            intersection_y
                        );
                    }
                }

                if (next_edge
                    && !half_edges_share_vertex(edge.edge, next_edge->edge)) {
                    LOG_DIAG(
                        TRACE,
                        DCEL,
                        "Intersect test: inserted (%.1f,%.1f -> %.1f,%.1f, %p) against next (%.1f,%.1f -> %.1f,%.1f, %p)",
                        edge.edge->origin->x,
                        edge.edge->origin->y,
                        edge.edge->twin->origin->x,
                        edge.edge->twin->origin->y,
                        (void*)edge.edge,
                        next_edge->edge->origin->x,
                        next_edge->edge->origin->y,
                        next_edge->edge->twin->origin->x,
                        next_edge->edge->twin->origin->y,
                        (void*)next_edge->edge
                    );

                    double intersection_x;
                    double intersection_y;
                    if (compute_intersection_point(
                            next_edge->edge,
                            edge.edge,
                            &intersection_x,
                            &intersection_y
                        )) {
                        dcel_intersect_edges(
                            dcel,
                            edge.edge,
                            next_edge->edge,
                            intersection_x,
                            intersection_y
                        );
                    }
                }
            }

            incident_edge = dcel_next_incident_edge(incident_edge);
        } while (incident_edge && incident_edge != event->data->incident_edge);

        LOG_DIAG(
            INFO,
            DCEL,
            "Current active edges list (%zu edges):",
            dcel_active_edges_len(active_edges)
        );
        for (size_t idx = 0; idx < dcel_active_edges_len(active_edges); idx++) {
            ActiveEdge edge;
            RELEASE_ASSERT(dcel_active_edges_get(active_edges, idx, &edge));

            LOG_DIAG(
                TRACE,
                DCEL,
                "Edge: %p, (%.1f,%.1f -> %.1f,%.1f)",
                (void*)edge.edge,
                edge.edge->origin->x,
                edge.edge->origin->y,
                edge.edge->twin->origin->x,
                edge.edge->twin->origin->y
            );
        }

        // debug_render(
        //     dcel,
        //     local_arena,
        //     event->data->x,
        //     event->data->y,
        //     active_edges,
        //     NULL,
        //     NULL,
        //     NULL
        // );

        event = event->next;
    }

    arena_free(local_arena);
}

void dcel_assign_faces(Dcel* dcel) {
    RELEASE_ASSERT(dcel);

    Arena* local_arena = arena_new(1024);
    DcelActiveEdges* active_edges = dcel_active_edges_new(local_arena);

    DcelEventQueueBlock* event = dcel->event_queue->front;
    while (event) {
        DcelHalfEdge* incident_edge = event->data->incident_edge;
        do {
            bool removed = false;
            for (size_t idx = 0; idx < dcel_active_edges_len(active_edges);
                 idx++) {
                ActiveEdge active_edge;
                RELEASE_ASSERT(
                    dcel_active_edges_get(active_edges, idx, &active_edge)
                );

                if (incident_edge->twin == active_edge.edge) {
                    dcel_active_edges_remove(active_edges, idx);
                    removed = true;
                    break;
                }
            }

            if (!removed) {
                dcel_active_edges_insert_sorted(
                    active_edges,
                    (ActiveEdge) {.edge = incident_edge, .helper = NULL},
                    active_edges_cmp,
                    event->data->y,
                    true
                );
            }

            incident_edge = dcel_next_incident_edge(incident_edge);
        } while (incident_edge && incident_edge != event->data->incident_edge);

        for (size_t idx = 0; idx < dcel_active_edges_len(active_edges); idx++) {
            ActiveEdge active_edge;
            RELEASE_ASSERT(
                dcel_active_edges_get(active_edges, idx, &active_edge)
            );

            DcelHalfEdge* left = active_edge.edge->twin;
            DcelHalfEdge* right = active_edge.edge;

            if (!left->face) {
                debug_render(
                    dcel,
                    local_arena,
                    event->data->x,
                    event->data->y,
                    active_edges,
                    left,
                    NULL,
                    NULL
                );

                if (idx == 0) {
                    left->face = dcel->outer_face;
                } else {
                    ActiveEdge prev_right_edge;
                    RELEASE_ASSERT(dcel_active_edges_get(
                        active_edges,
                        idx - 1,
                        &prev_right_edge
                    ));

                    DEBUG_ASSERT(prev_right_edge.edge->face);
                    left->face = prev_right_edge.edge->face;
                }

                DcelHalfEdge* current_edge = left;
                do {
                    debug_render(
                        dcel,
                        local_arena,
                        event->data->x,
                        event->data->y,
                        active_edges,
                        current_edge,
                        NULL,
                        NULL
                    );

                    current_edge->face = left->face;
                    current_edge = current_edge->next;
                } while (current_edge && current_edge != left);
            }

            if (!right->face) {
                debug_render(
                    dcel,
                    local_arena,
                    event->data->x,
                    event->data->y,
                    active_edges,
                    right,
                    NULL,
                    NULL
                );

                right->face = arena_alloc(dcel->arena, sizeof(DcelFace));
                dcel_faces_push(dcel->faces, right->face);

                DcelHalfEdge* current_edge = right;
                do {
                    debug_render(
                        dcel,
                        local_arena,
                        event->data->x,
                        event->data->y,
                        active_edges,
                        current_edge,
                        NULL,
                        NULL
                    );

                    current_edge->face = right->face;
                    current_edge = current_edge->next;
                } while (current_edge != right);
            }
        }

        // debug_render(
        //     dcel,
        //     local_arena,
        //     event->data->x,
        //     event->data->y,
        //     active_edges,
        //     NULL
        // );

        event = event->next;
    }

    arena_free(local_arena);
}

static uint32_t hash32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352d;
    x ^= x >> 15;
    x *= 0x846ca68b;
    x ^= x >> 16;
    return x;
}

static uint32_t color_from_ptr(void* ptr) {
    uintptr_t p = (uintptr_t)ptr;
    uint32_t hash = hash32((uint32_t)(p ^ (p >> 32)) + 1);

    uint8_t r = (hash >> 0) & 0xff;
    uint8_t g = (hash >> 8) & 0xff;
    uint8_t b = (hash >> 16) & 0xff;
    uint8_t a = 0xff;

    return ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8)
        | ((uint32_t)a << 0);
}

void dcel_render(
    Dcel* dcel,
    uint32_t rgba,
    Canvas* canvas,
    DcelHalfEdge* highlight_edge
) {
    RELEASE_ASSERT(dcel);
    RELEASE_ASSERT(canvas);

    for (size_t edge_idx = 0; edge_idx < dcel_half_edges_len(dcel->half_edges);
         edge_idx++) {
        DcelHalfEdge* half_edge;
        RELEASE_ASSERT(
            dcel_half_edges_get_ptr(dcel->half_edges, edge_idx, &half_edge)
        );

        double delta_x = half_edge->twin->origin->x - half_edge->origin->x;
        double delta_y = half_edge->twin->origin->y - half_edge->origin->y;
        double len = sqrt(delta_x * delta_x + delta_y * delta_y);
        if (len < 1e-9) {
            continue;
        }

        double normal_x = -delta_y / len;
        double normal_y = delta_x / len;

        double spacing = 2.0;
        double offset_x = normal_x * spacing;
        double offset_y = normal_y * spacing;

        double radius = 2.1;
        double tip_radius = 2.1;

        uint32_t color = half_edge == highlight_edge
            ? 0xff00ffff
            : (half_edge->face ? color_from_ptr(half_edge->face) : 0x808080ff);

        canvas_draw_arrow(
            canvas,
            half_edge->origin->x - offset_x,
            half_edge->origin->y - offset_y,
            half_edge->twin->origin->x - offset_x,
            half_edge->twin->origin->y - offset_y,
            radius,
            tip_radius,
            color
        );
    }

    for (size_t vertex_idx = 0; vertex_idx < dcel_vertices_len(dcel->vertices);
         vertex_idx++) {
        DcelVertex vertex;
        RELEASE_ASSERT(dcel_vertices_get(dcel->vertices, vertex_idx, &vertex));

        canvas_draw_circle(
            canvas,
            vertex.x,
            vertex.y,
            10.0,
            vertex.highlight ? 0xff00ffff : rgba
        );
    }
}
