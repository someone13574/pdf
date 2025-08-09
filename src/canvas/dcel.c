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

    ax = edge_intersect_x(lhs->edge, sweep_y + 1e-5);
    bx = edge_intersect_x(rhs->edge, sweep_y + 1e-5);

    return ax < bx;
}

DcelVertex* dcel_add_vertex(Dcel* dcel, double x, double y) {
    RELEASE_ASSERT(dcel);

    DcelVertex* vertex = dcel_vertices_push(
        dcel->vertices,
        (DcelVertex
        ) {.x = x, .y = y, .incident_edge = NULL, .merge = false, .split = false
        }
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
           .rendered = false}
    );

    DcelHalfEdge* half_edge_b = dcel_half_edges_push(
        dcel->half_edges,
        (DcelHalfEdge
        ) {.origin = b,
           .twin = NULL,
           .next = NULL,
           .prev = NULL,
           .rendered = false}
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

static void add_incident_angle(
    IncidentAnglesList* incident_angles,
    DcelHalfEdge* half_edge,
    DcelVertex* vertex
) {
    RELEASE_ASSERT(incident_angles);
    RELEASE_ASSERT(half_edge);
    RELEASE_ASSERT(vertex);

    double delta_x = half_edge->twin->origin->x - vertex->x;
    double delta_y = half_edge->twin->origin->y - vertex->y;
    double angle = atan2(delta_y, delta_x);

    incident_angles_list_insert_sorted(
        incident_angles,
        (IncidentAngle) {.half_edge = half_edge, .angle = angle},
        cmp_incident_angles_list,
        true
    );
}

static void rewire_incident_angles(
    IncidentAnglesList* incident_angles,
    DcelVertex* vertex
) {
    RELEASE_ASSERT(incident_angles);
    RELEASE_ASSERT(vertex);

    for (size_t idx_a = 0; idx_a < incident_angles_list_len(incident_angles);
         idx_a++) {
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
}

DcelVertex* dcel_intersect_edges(
    Dcel* dcel,
    DcelHalfEdge* a,
    DcelHalfEdge* b,
    double intersection_x,
    double intersection_y
) {
    LOG_DIAG(
        DEBUG,
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
        add_incident_angle(incident_angles, incident_edges[idx], vertex);
    }

    rewire_incident_angles(incident_angles, vertex);

    arena_free(local_arena);
    return vertex;
}

void dcel_connect_vertices(Dcel* dcel, DcelVertex* a, DcelVertex* b) {
    RELEASE_ASSERT(dcel);
    RELEASE_ASSERT(a);
    RELEASE_ASSERT(b);

    DcelHalfEdge* a_incident = a->incident_edge;
    DcelHalfEdge* b_incident = b->incident_edge;

    DcelHalfEdge* edge = dcel_add_edge(dcel, a, b);

    // Rewire vertex a
    Arena* local_arena = arena_new(128);

    IncidentAnglesList* incident_angles = incident_angles_list_new(local_arena);
    add_incident_angle(incident_angles, edge, a);

    DcelHalfEdge* incident = a_incident;
    do {
        add_incident_angle(incident_angles, incident, a);
        incident = dcel_next_incident_edge(incident);
    } while (incident && incident != a_incident);

    rewire_incident_angles(incident_angles, a);

    // Rewire vertex b
    incident_angles_list_clear(incident_angles);
    add_incident_angle(incident_angles, edge->twin, b);

    incident = b_incident;
    do {
        add_incident_angle(incident_angles, incident, b);
        incident = dcel_next_incident_edge(incident);
    } while (incident && incident != b_incident);

    rewire_incident_angles(incident_angles, b);

    // Assign face to a incident
    edge->face = edge->next->face;

    // Create new face
    DcelFace* new_face = arena_alloc(dcel->arena, sizeof(DcelFace));
    dcel_faces_push(dcel->faces, new_face);

    DcelHalfEdge* half_edge = edge->twin;
    do {
        half_edge->face = new_face;
        half_edge = half_edge->next;
    } while (half_edge && half_edge != edge->twin);

    arena_free(local_arena);
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
                        DEBUG,
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
                LOG_DIAG(DEBUG, DCEL, "Inserting edge %p", (void*)edge.edge);

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
            TRACE,
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

        event = event->next;
    }

    arena_free(local_arena);
}

static void debug_render(Dcel* dcel, Arena* arena) {
    RELEASE_ASSERT(dcel);
    RELEASE_ASSERT(arena);

    uint32_t resolution_multiplier = 2;
    Canvas* canvas = canvas_new(
        arena,
        1000 * resolution_multiplier,
        900 * resolution_multiplier,
        0xffffffff,
        (double)resolution_multiplier
    );

    dcel_render(dcel, canvas);
    canvas_write_file(canvas, "tessellation.bmp");
    usleep(100000);
}

static double signed_cycle_area(DcelHalfEdge* start_half_edge) {
    RELEASE_ASSERT(start_half_edge);
    RELEASE_ASSERT(start_half_edge->prev);

    DcelHalfEdge* half_edge = start_half_edge;
    DcelVertex* prev_point = start_half_edge->prev->origin;

    double signed_area = 0.0;

    // https://en.wikipedia.org/wiki/Shoelace_formula
    do {
        signed_area += prev_point->x * half_edge->origin->y
            - prev_point->y * half_edge->origin->x;

        prev_point = half_edge->origin;
        half_edge = half_edge->next;
    } while (half_edge && half_edge != start_half_edge);

    return signed_area / 2.0;
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

            DcelHalfEdge* left = active_edge.edge;
            DcelHalfEdge* right = active_edge.edge->twin;

            if (!left->face) {
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
                    current_edge->face = left->face;
                    current_edge = current_edge->next;
                    debug_render(dcel, local_arena);
                } while (current_edge && current_edge != left);

                LOG_DIAG(
                    INFO,
                    DCEL,
                    "Area (left): %f",
                    signed_cycle_area(left)
                );
            }

            if (!right->face) {
                right->face = arena_alloc(dcel->arena, sizeof(DcelFace));
                dcel_faces_push(dcel->faces, right->face);

                DcelHalfEdge* current_edge = right;
                do {
                    current_edge->face = right->face;
                    current_edge = current_edge->next;
                    debug_render(dcel, local_arena);
                } while (current_edge != right);

                LOG_DIAG(
                    INFO,
                    DCEL,
                    "Area (right): %f",
                    signed_cycle_area(right)
                );
            }
        }

        event = event->next;
    }

    arena_free(local_arena);
}

static void assign_vertex_types(Dcel* dcel) {
    RELEASE_ASSERT(dcel);

    for (size_t idx = 0; idx < dcel_vertices_len(dcel->vertices); idx++) {
        DcelVertex* vertex = NULL;
        RELEASE_ASSERT(dcel_vertices_get_ptr(dcel->vertices, idx, &vertex));

        vertex->merge = true;
        vertex->split = true;

        DcelHalfEdge* max_gap_edge = NULL;
        double max_gap = 0.0;
        double prev_angle = 0.0;
        double first_angle = 0.0;

        DcelHalfEdge* incident_edge = vertex->incident_edge;
        do {
            // Find gap size
            double angle = atan2(
                incident_edge->twin->origin->y - vertex->y,
                incident_edge->twin->origin->x - vertex->x
            );

            if (incident_edge != vertex->incident_edge) {
                double gap = angle - prev_angle;
                if (gap < 0.0) {
                    gap += 2.0 * M_PI;
                }

                if (gap > max_gap) {
                    max_gap_edge = incident_edge;
                    max_gap = gap;
                }
            } else {
                first_angle = angle;
            }

            prev_angle = angle;

            // Test if above or below vertex
            bool below_vertex = incident_edge->twin->origin->y < vertex->y;
            if (below_vertex) {
                vertex->split = false;
            } else {
                vertex->merge = false;
            }

            incident_edge = dcel_next_incident_edge(incident_edge);
        } while (incident_edge && incident_edge != vertex->incident_edge);

        double gap = first_angle - prev_angle;
        if (gap < 0.0) {
            gap += 2.0 * M_PI;
        }

        if (gap > max_gap) {
            max_gap_edge = vertex->incident_edge;
        }

        if (max_gap_edge && max_gap_edge->face == dcel->outer_face) {
            vertex->split = false;
            vertex->merge = false;
        }
    }
}

void dcel_partition(Dcel* dcel) {
    RELEASE_ASSERT(dcel);

    assign_vertex_types(dcel);

    Arena* local_arena = arena_new(1024);
    DcelActiveEdges* active_edges = dcel_active_edges_new(local_arena);

    DcelEventQueueBlock* event = dcel->event_queue->front;
    while (event) {
        DcelVertex* merge_helper = NULL;
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
                    // Remove from active set
                    dcel_active_edges_remove(active_edges, idx);
                    removed = true;

                    // Create edge with merge helper
                    if (active_edge.helper->merge) {
                        merge_helper = active_edge.helper;
                    }
                    break;
                }
            }

            if (!removed) {
                dcel_active_edges_insert_sorted(
                    active_edges,
                    (ActiveEdge) {.edge = incident_edge, .helper = event->data},
                    active_edges_cmp,
                    event->data->y,
                    true
                );
            }

            incident_edge = dcel_next_incident_edge(incident_edge);
        } while (incident_edge && incident_edge != event->data->incident_edge);

        if (merge_helper) {
            dcel_connect_vertices(dcel, event->data, merge_helper);
        }

        double project_x = -1.0;
        ActiveEdge* project_edge = NULL;
        for (size_t idx = 0; idx < dcel_active_edges_len(active_edges); idx++) {
            ActiveEdge* active_edge = NULL;
            RELEASE_ASSERT(
                dcel_active_edges_get_ptr(active_edges, idx, &active_edge)
            );

            double intersect_x =
                edge_intersect_x(active_edge->edge, event->data->y);
            if (active_edge->edge->origin != event->data
                && intersect_x > project_x && intersect_x < event->data->x) {
                project_x = intersect_x;
                project_edge = active_edge;
            }
        }

        if (project_edge) {
            if (project_edge->helper->merge || event->data->split) {
                dcel_connect_vertices(dcel, event->data, project_edge->helper);
            }

            project_edge->helper = event->data;
        }

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

static double point_left_of_segment(
    DcelVertex* vertex_a,
    DcelVertex* vertex_b,
    double x,
    double y
) {
    return (vertex_b->x - vertex_a->x) * (y - vertex_a->y)
        - (x - vertex_a->x) * (vertex_b->y - vertex_a->y);
}

static bool
point_in_polygon(DcelHalfEdge* start_half_edge, double x, double y) {
    RELEASE_ASSERT(start_half_edge);

    DcelHalfEdge* half_edge = start_half_edge;
    int winding = 0;

    do {
        DcelVertex* vertex_a = half_edge->origin;
        DcelVertex* vertex_b = half_edge->twin->origin;
        RELEASE_ASSERT(vertex_a);
        RELEASE_ASSERT(vertex_b);

        if (vertex_a->y <= y && vertex_b->y > y) {
            if (point_left_of_segment(vertex_a, vertex_b, x, y) > 0) {
                winding++;
            }
        }

        if (vertex_a->y > y && vertex_b->y <= y) {
            if (point_left_of_segment(vertex_a, vertex_b, x, y) < 0) {
                winding--;
            }
        }

        half_edge = half_edge->next;
    } while (half_edge && half_edge != start_half_edge);

    return winding % 2 != 0;
}

void dcel_render(Dcel* dcel, Canvas* canvas) {
    RELEASE_ASSERT(dcel);
    RELEASE_ASSERT(canvas);

    for (size_t idx = 0; idx < dcel_half_edges_len(dcel->half_edges); idx++) {
        DcelHalfEdge* half_edge = NULL;
        RELEASE_ASSERT(
            dcel_half_edges_get_ptr(dcel->half_edges, idx, &half_edge)
        );

        if (half_edge->rendered) {
            continue;
        }

        double signed_area = signed_cycle_area(half_edge);

        DcelHalfEdge* curr_half_edge = half_edge;
        do {
            double delta_x =
                curr_half_edge->twin->origin->x - curr_half_edge->origin->x;
            double delta_y =
                curr_half_edge->twin->origin->y - curr_half_edge->origin->y;
            double len = sqrt(delta_x * delta_x + delta_y * delta_y);
            if (len < 1e-9) {
                curr_half_edge->rendered = true;
                curr_half_edge = curr_half_edge->next;
                continue;
            }

            double normal_x = -delta_y / len;
            double normal_y = delta_x / len;

            double spacing = 5.0;
            double face_sign = signed_area < 0.0 ? -1.0 : 1.0;
            double offset_x = normal_x * spacing;
            double offset_y = normal_y * spacing;

            bool in_poly = point_in_polygon(
                curr_half_edge,
                (curr_half_edge->origin->x + curr_half_edge->twin->origin->x)
                        / 2.0
                    + offset_x * 1e-2,
                (curr_half_edge->origin->y + curr_half_edge->twin->origin->y)
                        / 2.0
                    + offset_y * 1e-2
            );
            // double sign = in_poly ? 1.0 : -1.0;

            double radius = 5.1;
            double tip_radius = 1.0;

            // uint32_t color = face_sign < 0.0 ? 0xff0000ff : 0x00ff00ff;
            uint32_t color = color_from_ptr((void*)curr_half_edge);

            canvas_draw_circle(
                canvas,
                (curr_half_edge->origin->x + curr_half_edge->twin->origin->x)
                        / 2.0
                    + offset_x * 2,
                (curr_half_edge->origin->y + curr_half_edge->twin->origin->y)
                        / 2.0
                    + offset_y * 2,
                8.0,
                (face_sign > 0.0) == in_poly ? 0x00ff00ff : 0xff0000ff
            );

            canvas_draw_circle(
                canvas,
                (curr_half_edge->origin->x + curr_half_edge->twin->origin->x)
                        / 2.0
                    + offset_x * 2,
                (curr_half_edge->origin->y + curr_half_edge->twin->origin->y)
                        / 2.0
                    + offset_y * 2,
                5.0,
                color
            );

            canvas_draw_arrow(
                canvas,
                curr_half_edge->origin->x + offset_x,
                curr_half_edge->origin->y + offset_y,
                curr_half_edge->twin->origin->x + offset_x,
                curr_half_edge->twin->origin->y + offset_y,
                radius,
                tip_radius,
                color
            );

            curr_half_edge->rendered = true;
            curr_half_edge = curr_half_edge->next;
        } while (curr_half_edge && curr_half_edge != half_edge);
    }

    for (size_t idx = 0; idx < dcel_half_edges_len(dcel->half_edges); idx++) {
        DcelHalfEdge* half_edge = NULL;
        RELEASE_ASSERT(
            dcel_half_edges_get_ptr(dcel->half_edges, idx, &half_edge)
        );

        half_edge->rendered = false;
    }
}
