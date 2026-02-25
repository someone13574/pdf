#include "dcel.h"

#define _USE_MATH_DEFINES
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "arena/arena.h"
#include "logger/log.h"
#include "path_builder.h"
#include "raster_canvas.h"

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
        (DcelVertex) {.x = x,
                      .y = y,
                      .incident_edge = NULL,
                      .merge = false,
                      .split = false}
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
        (DcelHalfEdge) {.origin = a,
                        .twin = NULL,
                        .next = NULL,
                        .prev = NULL,
                        .rendered = false}
    );

    DcelHalfEdge* half_edge_b = dcel_half_edges_push(
        dcel->half_edges,
        (DcelHalfEdge) {.origin = b,
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
        RELEASE_ASSERT(
            incident_angles_list_get(incident_angles, idx_a, &edge_a)
        );
        RELEASE_ASSERT(
            incident_angles_list_get(incident_angles, idx_b, &edge_b)
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

DcelHalfEdge* dcel_next_incident_edge(const DcelHalfEdge* half_edge) {
    RELEASE_ASSERT(half_edge);

    return half_edge->twin->next;
}

static bool half_edges_share_vertex(DcelHalfEdge* a, DcelHalfEdge* b) {
    RELEASE_ASSERT(a);
    RELEASE_ASSERT(b);

    const double eps = 1e-9;
    GeomVec2 a_from = geom_vec2_new(a->origin->x, a->origin->y);
    GeomVec2 a_to = geom_vec2_new(a->twin->origin->x, a->twin->origin->y);
    GeomVec2 b_from = geom_vec2_new(b->origin->x, b->origin->y);
    GeomVec2 b_to = geom_vec2_new(b->twin->origin->x, b->twin->origin->y);

    return geom_vec2_equal_eps(a_from, b_from, eps)
        || geom_vec2_equal_eps(a_from, b_to, eps)
        || geom_vec2_equal_eps(a_to, b_from, eps)
        || geom_vec2_equal_eps(a_to, b_to, eps);
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

    // Only split on proper interior intersections. Endpoint touches are
    // already represented as vertices and repeatedly splitting them can
    // explode edge counts on shared/touching contours.
    const double eps = 1e-9;
    if (ua <= eps || ua >= 1.0 - eps || ub <= eps || ub >= 1.0 - eps) {
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
                } while (current_edge && current_edge != left);

                LOG_DIAG(
                    TRACE,
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
                } while (current_edge != right);

                LOG_DIAG(
                    TRACE,
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

static bool dcel_sample_on_segment(
    GeomVec2 a,
    GeomVec2 b,
    double sample_x,
    double sample_y
) {
    const double eps = 1e-5;

    GeomVec2 ab = geom_vec2_sub(b, a);
    GeomVec2 ap = geom_vec2_new(sample_x - a.x, sample_y - a.y);
    double ab_len_sq = geom_vec2_len_sq(ab);
    if (ab_len_sq <= 1e-18) {
        return geom_vec2_len_sq(ap) <= eps * eps;
    }

    double cross = ap.x * ab.y - ap.y * ab.x;
    if (cross * cross > eps * eps * ab_len_sq) {
        return false;
    }

    double dot = geom_vec2_dot(ap, ab);
    if (dot < -eps || dot > ab_len_sq + eps) {
        return false;
    }

    return true;
}

static void dcel_update_crossing(
    GeomVec2 a,
    GeomVec2 b,
    double sample_x,
    double sample_y,
    int* winding,
    int* parity,
    bool* on_boundary
) {
    RELEASE_ASSERT(winding);
    RELEASE_ASSERT(parity);
    RELEASE_ASSERT(on_boundary);

    if (*on_boundary) {
        return;
    }
    if (dcel_sample_on_segment(a, b, sample_x, sample_y)) {
        *on_boundary = true;
        return;
    }

    bool crosses_up = a.y <= sample_y && b.y > sample_y;
    bool crosses_down = a.y > sample_y && b.y <= sample_y;
    if (!crosses_up && !crosses_down) {
        return;
    }

    double y_delta = b.y - a.y;
    if (fabs(y_delta) < 1e-18) {
        return;
    }

    double t = (sample_y - a.y) / y_delta;
    double x_intersection = a.x + t * (b.x - a.x);
    if (x_intersection <= sample_x) {
        return;
    }

    *parity ^= 1;
    *winding += crosses_up ? 1 : -1;
}

bool dcel_path_contains_point(
    const PathBuilder* path,
    DcelFillRule fill_rule,
    double x,
    double y
) {
    RELEASE_ASSERT(path);

    int winding = 0;
    int parity = 0;
    bool on_boundary = false;

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
        RELEASE_ASSERT(
            first.type == PATH_CONTOUR_SEGMENT_TYPE_START,
            "Path contour must start with START segment"
        );

        GeomVec2 start = first.value.start;
        GeomVec2 current = start;
        bool has_line = false;

        for (size_t segment_idx = 1; segment_idx < path_contour_len(contour);
             segment_idx++) {
            PathContourSegment segment;
            RELEASE_ASSERT(path_contour_get(contour, segment_idx, &segment));

            switch (segment.type) {
                case PATH_CONTOUR_SEGMENT_TYPE_START: {
                    start = segment.value.start;
                    current = segment.value.start;
                    has_line = false;
                    break;
                }
                case PATH_CONTOUR_SEGMENT_TYPE_LINE: {
                    dcel_update_crossing(
                        current,
                        segment.value.line,
                        x,
                        y,
                        &winding,
                        &parity,
                        &on_boundary
                    );
                    current = segment.value.line;
                    has_line = true;
                    break;
                }
                case PATH_CONTOUR_SEGMENT_TYPE_QUAD_BEZIER: {
                    RELEASE_ASSERT(
                        false,
                        "DCEL point test requires flattened path segments"
                    );
                    break;
                }
                case PATH_CONTOUR_SEGMENT_TYPE_CUBIC_BEZIER: {
                    RELEASE_ASSERT(
                        false,
                        "DCEL point test requires flattened path segments"
                    );
                    break;
                }
            }

            if (on_boundary) {
                break;
            }
        }

        if (!on_boundary && has_line
            && !geom_vec2_equal_eps(current, start, 1e-9)) {
            dcel_update_crossing(
                current,
                start,
                x,
                y,
                &winding,
                &parity,
                &on_boundary
            );
        }

        if (on_boundary) {
            break;
        }
    }

    return on_boundary || (fill_rule == DCEL_FILL_RULE_EVEN_ODD ? parity != 0
                                                                 : winding != 0);
}

static int dcel_cmp_double(const void* lhs, const void* rhs) {
    double a = *(const double*)lhs;
    double b = *(const double*)rhs;
    return (a > b) - (a < b);
}

static size_t dcel_collect_contour_points(
    const PathContour* contour,
    GeomVec2* points,
    size_t points_cap
) {
    RELEASE_ASSERT(contour);
    RELEASE_ASSERT(points);
    RELEASE_ASSERT(points_cap > 0);

    if (path_contour_len(contour) == 0) {
        return 0;
    }

    PathContourSegment first;
    RELEASE_ASSERT(path_contour_get(contour, 0, &first));
    RELEASE_ASSERT(
        first.type == PATH_CONTOUR_SEGMENT_TYPE_START,
        "Path contour must start with START segment"
    );

    size_t point_count = 0;
    points[point_count++] = first.value.start;
    for (size_t segment_idx = 1; segment_idx < path_contour_len(contour);
         segment_idx++) {
        PathContourSegment segment;
        RELEASE_ASSERT(path_contour_get(contour, segment_idx, &segment));

        switch (segment.type) {
            case PATH_CONTOUR_SEGMENT_TYPE_START: {
                RELEASE_ASSERT(false, "Unexpected START segment in contour");
                break;
            }
            case PATH_CONTOUR_SEGMENT_TYPE_LINE: {
                RELEASE_ASSERT(point_count < points_cap);
                points[point_count++] = segment.value.line;
                break;
            }
            case PATH_CONTOUR_SEGMENT_TYPE_QUAD_BEZIER: {
                RELEASE_ASSERT(
                    false,
                    "DCEL rasterization requires flattened path segments"
                );
                break;
            }
            case PATH_CONTOUR_SEGMENT_TYPE_CUBIC_BEZIER: {
                RELEASE_ASSERT(
                    false,
                    "DCEL rasterization requires flattened path segments"
                );
                break;
            }
        }
    }

    while (point_count > 1
           && geom_vec2_equal_eps(points[point_count - 1], points[0], 1e-9)) {
        point_count--;
    }

    if (point_count <= 1) {
        return point_count;
    }

    size_t write_idx = 1;
    for (size_t read_idx = 1; read_idx < point_count; read_idx++) {
        if (geom_vec2_equal_eps(points[read_idx], points[write_idx - 1], 1e-9)) {
            continue;
        }
        points[write_idx++] = points[read_idx];
    }
    point_count = write_idx;

    while (point_count > 1
           && geom_vec2_equal_eps(points[point_count - 1], points[0], 1e-9)) {
        point_count--;
    }

    return point_count;
}

static bool dcel_build_from_path(Dcel* dcel, const PathBuilder* path) {
    RELEASE_ASSERT(dcel);
    RELEASE_ASSERT(path);

    bool has_edges = false;

    for (size_t contour_idx = 0;
         contour_idx < path_contour_vec_len(path->contours);
         contour_idx++) {
        PathContour* contour = NULL;
        RELEASE_ASSERT(
            path_contour_vec_get(path->contours, contour_idx, &contour)
        );

        size_t segment_count = path_contour_len(contour);
        if (segment_count < 2) {
            continue;
        }

        GeomVec2* points = arena_alloc(
            dcel->arena,
            segment_count * sizeof(GeomVec2)
        );
        size_t point_count =
            dcel_collect_contour_points(contour, points, segment_count);
        if (point_count < 3) {
            continue;
        }

        DcelVertex* first_vertex =
            dcel_add_vertex(dcel, points[0].x, points[0].y);
        DcelVertex* prev_vertex = first_vertex;
        DcelHalfEdge* first_edge = NULL;
        DcelHalfEdge* prev_edge = NULL;

        for (size_t point_idx = 1; point_idx < point_count; point_idx++) {
            DcelVertex* next_vertex = dcel_add_vertex(
                dcel,
                points[point_idx].x,
                points[point_idx].y
            );
            DcelHalfEdge* half_edge =
                dcel_add_edge(dcel, prev_vertex, next_vertex);

            if (first_edge) {
                prev_edge->next = half_edge;
                half_edge->prev = prev_edge;

                prev_edge->twin->prev = half_edge->twin;
                half_edge->twin->next = prev_edge->twin;
            } else {
                first_edge = half_edge;
            }

            prev_vertex = next_vertex;
            prev_edge = half_edge;
        }

        RELEASE_ASSERT(first_edge);
        RELEASE_ASSERT(prev_edge);

        DcelHalfEdge* closing_edge = dcel_add_edge(dcel, prev_vertex, first_vertex);
        first_edge->prev = closing_edge;
        prev_edge->next = closing_edge;
        closing_edge->next = first_edge;
        closing_edge->prev = prev_edge;

        first_edge->twin->next = closing_edge->twin;
        prev_edge->twin->prev = closing_edge->twin;
        closing_edge->twin->next = prev_edge->twin;
        closing_edge->twin->prev = first_edge->twin;

        has_edges = true;
    }

    return has_edges;
}

static size_t dcel_cycle_len(const DcelHalfEdge* start_half_edge) {
    RELEASE_ASSERT(start_half_edge);

    size_t len = 0;
    const DcelHalfEdge* half_edge = start_half_edge;
    do {
        len++;
        half_edge = half_edge->next;
    } while (half_edge && half_edge != start_half_edge);

    return half_edge ? len : 0;
}

static void dcel_cycle_bounds(
    const DcelHalfEdge* start_half_edge,
    double* out_min_x,
    double* out_min_y,
    double* out_max_x,
    double* out_max_y
) {
    RELEASE_ASSERT(start_half_edge);
    RELEASE_ASSERT(out_min_x);
    RELEASE_ASSERT(out_min_y);
    RELEASE_ASSERT(out_max_x);
    RELEASE_ASSERT(out_max_y);

    const DcelHalfEdge* half_edge = start_half_edge;
    *out_min_x = half_edge->origin->x;
    *out_min_y = half_edge->origin->y;
    *out_max_x = half_edge->origin->x;
    *out_max_y = half_edge->origin->y;

    do {
        double x = half_edge->origin->x;
        double y = half_edge->origin->y;
        if (x < *out_min_x) {
            *out_min_x = x;
        }
        if (y < *out_min_y) {
            *out_min_y = y;
        }
        if (x > *out_max_x) {
            *out_max_x = x;
        }
        if (y > *out_max_y) {
            *out_max_y = y;
        }

        half_edge = half_edge->next;
    } while (half_edge && half_edge != start_half_edge);
}

static size_t dcel_cycle_x_intersections(
    const DcelHalfEdge* start_half_edge,
    double sample_y,
    double* intersections,
    size_t intersections_cap
) {
    RELEASE_ASSERT(start_half_edge);
    RELEASE_ASSERT(intersections);

    size_t intersections_len = 0;
    const DcelHalfEdge* half_edge = start_half_edge;
    do {
        GeomVec2 a = geom_vec2_new(half_edge->origin->x, half_edge->origin->y);
        GeomVec2 b = geom_vec2_new(
            half_edge->twin->origin->x,
            half_edge->twin->origin->y
        );

        bool crosses = (a.y <= sample_y && b.y > sample_y)
                    || (a.y > sample_y && b.y <= sample_y);
        if (crosses) {
            double y_delta = b.y - a.y;
            if (fabs(y_delta) > 1e-18) {
                RELEASE_ASSERT(intersections_len < intersections_cap);
                double t = (sample_y - a.y) / y_delta;
                intersections[intersections_len++] = a.x + t * (b.x - a.x);
            }
        }

        half_edge = half_edge->next;
    } while (half_edge && half_edge != start_half_edge);

    return intersections_len;
}

static void dcel_cycle_mark_rendered(DcelHalfEdge* start_half_edge) {
    RELEASE_ASSERT(start_half_edge);

    DcelHalfEdge* half_edge = start_half_edge;
    do {
        half_edge->rendered = true;
        half_edge = half_edge->next;
    } while (half_edge && half_edge != start_half_edge);
}

static bool dcel_path_raster_bounds(
    const PathBuilder* path,
    uint32_t width,
    uint32_t height,
    double coordinate_scale,
    uint32_t* out_min_x,
    uint32_t* out_min_y,
    uint32_t* out_max_x,
    uint32_t* out_max_y
) {
    RELEASE_ASSERT(path);
    RELEASE_ASSERT(out_min_x);
    RELEASE_ASSERT(out_min_y);
    RELEASE_ASSERT(out_max_x);
    RELEASE_ASSERT(out_max_y);

    bool has_points = false;
    double min_x = 0.0;
    double min_y = 0.0;
    double max_x = 0.0;
    double max_y = 0.0;

    for (size_t contour_idx = 0;
         contour_idx < path_contour_vec_len(path->contours);
         contour_idx++) {
        PathContour* contour = NULL;
        RELEASE_ASSERT(
            path_contour_vec_get(path->contours, contour_idx, &contour)
        );

        for (size_t segment_idx = 0; segment_idx < path_contour_len(contour);
             segment_idx++) {
            PathContourSegment segment;
            RELEASE_ASSERT(path_contour_get(contour, segment_idx, &segment));

            GeomVec2 point;
            switch (segment.type) {
                case PATH_CONTOUR_SEGMENT_TYPE_START: {
                    point = segment.value.start;
                    break;
                }
                case PATH_CONTOUR_SEGMENT_TYPE_LINE: {
                    point = segment.value.line;
                    break;
                }
                case PATH_CONTOUR_SEGMENT_TYPE_QUAD_BEZIER: {
                    RELEASE_ASSERT(
                        false,
                        "DCEL rasterization requires flattened path segments"
                    );
                    continue;
                }
                case PATH_CONTOUR_SEGMENT_TYPE_CUBIC_BEZIER: {
                    RELEASE_ASSERT(
                        false,
                        "DCEL rasterization requires flattened path segments"
                    );
                    continue;
                }
            }

            if (!has_points) {
                min_x = point.x;
                min_y = point.y;
                max_x = point.x;
                max_y = point.y;
                has_points = true;
                continue;
            }

            if (point.x < min_x) {
                min_x = point.x;
            }
            if (point.y < min_y) {
                min_y = point.y;
            }
            if (point.x > max_x) {
                max_x = point.x;
            }
            if (point.y > max_y) {
                max_y = point.y;
            }
        }
    }

    if (!has_points || width == 0 || height == 0) {
        return false;
    }

    int64_t start_x = (int64_t)floor(min_x * coordinate_scale) - 1;
    int64_t start_y = (int64_t)floor(min_y * coordinate_scale) - 1;
    int64_t end_x = (int64_t)ceil(max_x * coordinate_scale);
    int64_t end_y = (int64_t)ceil(max_y * coordinate_scale);

    if (start_x < 0) {
        start_x = 0;
    }
    if (start_y < 0) {
        start_y = 0;
    }
    if (end_x >= (int64_t)width) {
        end_x = (int64_t)width - 1;
    }
    if (end_y >= (int64_t)height) {
        end_y = (int64_t)height - 1;
    }
    if (start_x > end_x || start_y > end_y) {
        return false;
    }

    *out_min_x = (uint32_t)start_x;
    *out_min_y = (uint32_t)start_y;
    *out_max_x = (uint32_t)end_x;
    *out_max_y = (uint32_t)end_y;
    return true;
}

typedef struct {
    bool has_pixels;
    uint32_t min_x;
    uint32_t min_y;
    uint32_t max_x;
    uint32_t max_y;
} DcelMaskAccum;

static void dcel_mask_accum_mark(
    DcelMaskAccum* accum,
    uint32_t x,
    uint32_t y
) {
    RELEASE_ASSERT(accum);

    if (!accum->has_pixels) {
        accum->has_pixels = true;
        accum->min_x = x;
        accum->min_y = y;
        accum->max_x = x;
        accum->max_y = y;
        return;
    }

    if (x < accum->min_x) {
        accum->min_x = x;
    }
    if (y < accum->min_y) {
        accum->min_y = y;
    }
    if (x > accum->max_x) {
        accum->max_x = x;
    }
    if (y > accum->max_y) {
        accum->max_y = y;
    }
}

static void dcel_rasterize_cycle(
    Arena* arena,
    const DcelHalfEdge* start_half_edge,
    const PathBuilder* path,
    DcelFillRule fill_rule,
    uint32_t width,
    uint32_t height,
    double coordinate_scale,
    uint8_t* out_mask,
    DcelMaskAccum* out_accum
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(start_half_edge);
    RELEASE_ASSERT(path);
    RELEASE_ASSERT(out_mask);
    RELEASE_ASSERT(out_accum);

    size_t cycle_len = dcel_cycle_len(start_half_edge);
    if (cycle_len < 3) {
        return;
    }

    double min_x = 0.0;
    double min_y = 0.0;
    double max_x = 0.0;
    double max_y = 0.0;
    dcel_cycle_bounds(start_half_edge, &min_x, &min_y, &max_x, &max_y);

    int64_t start_y = (int64_t)floor(min_y * coordinate_scale) - 1;
    int64_t end_y = (int64_t)ceil(max_y * coordinate_scale);
    if (start_y < 0) {
        start_y = 0;
    }
    if (end_y >= (int64_t)height) {
        end_y = (int64_t)height - 1;
    }
    if (start_y > end_y) {
        return;
    }

    double* intersections = arena_alloc(arena, cycle_len * sizeof(double));
    const double eps = 1e-9;
    for (int64_t py = start_y; py <= end_y; py++) {
        double sample_y = ((double)py + 0.5) / coordinate_scale;
        size_t intersections_len = dcel_cycle_x_intersections(
            start_half_edge,
            sample_y,
            intersections,
            cycle_len
        );
        if (intersections_len < 2) {
            continue;
        }

        qsort(intersections, intersections_len, sizeof(double), dcel_cmp_double);
        for (size_t idx = 0; idx + 1 < intersections_len; idx += 2) {
            double x0 = intersections[idx];
            double x1 = intersections[idx + 1];
            if (x1 < x0) {
                double tmp = x0;
                x0 = x1;
                x1 = tmp;
            }

            int64_t start_x = (int64_t)ceil((x0 - eps) * coordinate_scale - 0.5);
            int64_t end_x = (int64_t)floor((x1 + eps) * coordinate_scale - 0.5);
            if (start_x < 0) {
                start_x = 0;
            }
            if (end_x >= (int64_t)width) {
                end_x = (int64_t)width - 1;
            }
            if (start_x > end_x) {
                continue;
            }

            for (int64_t px = start_x; px <= end_x; px++) {
                size_t mask_idx =
                    (size_t)py * (size_t)width + (size_t)px;
                if (out_mask[mask_idx] != 0) {
                    continue;
                }

                double sample_x = ((double)px + 0.5) / coordinate_scale;
                if (!dcel_path_contains_point(path, fill_rule, sample_x, sample_y)) {
                    continue;
                }

                out_mask[mask_idx] = 1;
                dcel_mask_accum_mark(out_accum, (uint32_t)px, (uint32_t)py);
            }
        }
    }

    // Boundary samples can be missed by span filling on tangential rows.
    // Cover them explicitly by testing pixel centers near each cycle edge.
    const DcelHalfEdge* half_edge = start_half_edge;
    do {
        GeomVec2 a = geom_vec2_new(half_edge->origin->x, half_edge->origin->y);
        GeomVec2 b = geom_vec2_new(
            half_edge->twin->origin->x,
            half_edge->twin->origin->y
        );

        int64_t edge_start_x =
            (int64_t)floor(fmin(a.x, b.x) * coordinate_scale) - 1;
        int64_t edge_end_x =
            (int64_t)ceil(fmax(a.x, b.x) * coordinate_scale) + 1;
        int64_t edge_start_y =
            (int64_t)floor(fmin(a.y, b.y) * coordinate_scale) - 1;
        int64_t edge_end_y =
            (int64_t)ceil(fmax(a.y, b.y) * coordinate_scale) + 1;

        if (edge_start_x < 0) {
            edge_start_x = 0;
        }
        if (edge_start_y < 0) {
            edge_start_y = 0;
        }
        if (edge_end_x >= (int64_t)width) {
            edge_end_x = (int64_t)width - 1;
        }
        if (edge_end_y >= (int64_t)height) {
            edge_end_y = (int64_t)height - 1;
        }

        for (int64_t py = edge_start_y; py <= edge_end_y; py++) {
            double sample_y = ((double)py + 0.5) / coordinate_scale;
            for (int64_t px = edge_start_x; px <= edge_end_x; px++) {
                size_t mask_idx = (size_t)py * (size_t)width + (size_t)px;
                if (out_mask[mask_idx] != 0) {
                    continue;
                }

                double sample_x = ((double)px + 0.5) / coordinate_scale;
                if (!dcel_sample_on_segment(a, b, sample_x, sample_y)) {
                    continue;
                }
                if (!dcel_path_contains_point(path, fill_rule, sample_x, sample_y)) {
                    continue;
                }

                out_mask[mask_idx] = 1;
                dcel_mask_accum_mark(out_accum, (uint32_t)px, (uint32_t)py);
            }
        }

        half_edge = half_edge->next;
    } while (half_edge && half_edge != start_half_edge);
}

void dcel_rasterize_path_mask(
    Arena* arena,
    const PathBuilder* path,
    DcelFillRule fill_rule,
    uint32_t width,
    uint32_t height,
    double coordinate_scale,
    uint8_t* out_mask,
    DcelMaskBounds* out_bounds
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(path);
    RELEASE_ASSERT(out_mask);
    RELEASE_ASSERT(coordinate_scale > 1e-6);

    if (out_bounds) {
        *out_bounds = (DcelMaskBounds) {.is_empty = true,
                                        .min_x = 0,
                                        .min_y = 0,
                                        .max_x = 0,
                                        .max_y = 0};
    }

    if (width == 0 || height == 0) {
        return;
    }

    size_t pixel_count = (size_t)width * (size_t)height;
    memset(out_mask, 0, pixel_count);

    Dcel* dcel = dcel_new(arena);
    if (!dcel_build_from_path(dcel, path)) {
        return;
    }

    dcel_overlay(dcel);
    dcel_assign_faces(dcel);
    dcel_partition(dcel);

    DcelMaskAccum accum = {.has_pixels = false,
                           .min_x = 0,
                           .min_y = 0,
                           .max_x = 0,
                           .max_y = 0};

    for (size_t half_edge_idx = 0; half_edge_idx < dcel_half_edges_len(dcel->half_edges);
         half_edge_idx++) {
        DcelHalfEdge* half_edge = NULL;
        RELEASE_ASSERT(
            dcel_half_edges_get_ptr(dcel->half_edges, half_edge_idx, &half_edge)
        );
        half_edge->rendered = false;
    }

    for (size_t half_edge_idx = 0; half_edge_idx < dcel_half_edges_len(dcel->half_edges);
         half_edge_idx++) {
        DcelHalfEdge* half_edge = NULL;
        RELEASE_ASSERT(
            dcel_half_edges_get_ptr(dcel->half_edges, half_edge_idx, &half_edge)
        );

        if (half_edge->rendered) {
            continue;
        }

        dcel_rasterize_cycle(
            arena,
            half_edge,
            path,
            fill_rule,
            width,
            height,
            coordinate_scale,
            out_mask,
            &accum
        );

        dcel_cycle_mark_rendered(half_edge);
    }

    for (size_t half_edge_idx = 0; half_edge_idx < dcel_half_edges_len(dcel->half_edges);
         half_edge_idx++) {
        DcelHalfEdge* half_edge = NULL;
        RELEASE_ASSERT(
            dcel_half_edges_get_ptr(dcel->half_edges, half_edge_idx, &half_edge)
        );
        half_edge->rendered = false;
    }

    uint32_t bounds_min_x = 0;
    uint32_t bounds_min_y = 0;
    uint32_t bounds_max_x = 0;
    uint32_t bounds_max_y = 0;
    bool has_bounds = dcel_path_raster_bounds(
        path,
        width,
        height,
        coordinate_scale,
        &bounds_min_x,
        &bounds_min_y,
        &bounds_max_x,
        &bounds_max_y
    );
    if (has_bounds) {
        for (uint32_t py = bounds_min_y; py <= bounds_max_y; py++) {
            double sample_y = ((double)py + 0.5) / coordinate_scale;
            for (uint32_t px = bounds_min_x; px <= bounds_max_x; px++) {
                size_t mask_idx = (size_t)py * (size_t)width + (size_t)px;
                if (out_mask[mask_idx] != 0) {
                    continue;
                }

                double sample_x = ((double)px + 0.5) / coordinate_scale;
                if (!dcel_path_contains_point(path, fill_rule, sample_x, sample_y)) {
                    continue;
                }

                out_mask[mask_idx] = 1;
                dcel_mask_accum_mark(&accum, px, py);
            }
        }
    }

    if (out_bounds) {
        *out_bounds = (DcelMaskBounds) {.is_empty = !accum.has_pixels,
                                        .min_x = accum.min_x,
                                        .min_y = accum.min_y,
                                        .max_x = accum.max_x,
                                        .max_y = accum.max_y};
    }
}

static uint32_t hash32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352d;
    x ^= x >> 15;
    x *= 0x846ca68b;
    x ^= x >> 16;
    return x;
}

static Rgba color_from_ptr(void* ptr) {
    uintptr_t p = (uintptr_t)ptr;
    uint32_t hash = hash32((uint32_t)(p ^ (p >> 32)) + 1);

    uint8_t r = (hash >> 0) & 0xff;
    uint8_t g = (hash >> 8) & 0xff;
    uint8_t b = (hash >> 16) & 0xff;
    return rgba_new(
        (double)r / 255.0,
        (double)g / 255.0,
        (double)b / 255.0,
        1.0
    );
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

void dcel_render(const Dcel* dcel, RasterCanvas* canvas) {
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

            // Rgba color = face_sign < 0.0 ? rgba_new(1.0, 0.0, 0.0, 1.0)
            //                                : rgba_new(0.0, 1.0, 0.0, 1.0);
            Rgba color = color_from_ptr((void*)curr_half_edge);

            raster_canvas_draw_circle(
                canvas,
                (curr_half_edge->origin->x + curr_half_edge->twin->origin->x)
                        / 2.0
                    + offset_x * 2,
                (curr_half_edge->origin->y + curr_half_edge->twin->origin->y)
                        / 2.0
                    + offset_y * 2,
                8.0,
                (face_sign > 0.0) == in_poly ? rgba_new(0.0, 1.0, 0.0, 1.0)
                                             : rgba_new(1.0, 0.0, 0.0, 1.0)
            );

            raster_canvas_draw_circle(
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

            raster_canvas_draw_arrow(
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

#ifdef TEST

#include "test/test.h"

static void dcel_test_add_rect(
    PathBuilder* path,
    double min_x,
    double min_y,
    double max_x,
    double max_y,
    bool clockwise
) {
    RELEASE_ASSERT(path);

    path_builder_new_contour(path, geom_vec2_new(min_x, min_y));
    if (clockwise) {
        path_builder_line_to(path, geom_vec2_new(min_x, max_y));
        path_builder_line_to(path, geom_vec2_new(max_x, max_y));
        path_builder_line_to(path, geom_vec2_new(max_x, min_y));
    } else {
        path_builder_line_to(path, geom_vec2_new(max_x, min_y));
        path_builder_line_to(path, geom_vec2_new(max_x, max_y));
        path_builder_line_to(path, geom_vec2_new(min_x, max_y));
    }
    path_builder_close_contour(path);
}

static void dcel_test_add_polygon(
    PathBuilder* path,
    const GeomVec2* points,
    size_t point_count
) {
    RELEASE_ASSERT(path);
    RELEASE_ASSERT(points);
    RELEASE_ASSERT(point_count >= 3);

    path_builder_new_contour(path, points[0]);
    for (size_t idx = 1; idx < point_count; idx++) {
        path_builder_line_to(path, points[idx]);
    }
    path_builder_close_contour(path);
}

static uint32_t dcel_test_lcg_next(uint32_t* state) {
    RELEASE_ASSERT(state);
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

static double dcel_test_lcg_unit(uint32_t* state) {
    uint32_t value = dcel_test_lcg_next(state) & 0x00ffffffu;
    return (double)value / (double)0x01000000u;
}

static double dcel_test_quantize(double value, double step) {
    RELEASE_ASSERT(step > 0.0);
    return round(value / step) * step;
}

static bool dcel_test_path_contains_point_reference(
    const PathBuilder* path,
    DcelFillRule fill_rule,
    double x,
    double y
) {
    RELEASE_ASSERT(path);

    const long double two_pi = 2.0L * acosl(-1.0L);
    long double total_angle = 0.0L;

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

        GeomVec2 start = first.value.start;
        GeomVec2 current = start;
        bool has_line = false;

        for (size_t segment_idx = 1; segment_idx < path_contour_len(contour);
             segment_idx++) {
            PathContourSegment segment;
            RELEASE_ASSERT(path_contour_get(contour, segment_idx, &segment));
            RELEASE_ASSERT(segment.type == PATH_CONTOUR_SEGMENT_TYPE_LINE);

            GeomVec2 end = segment.value.line;
            if (dcel_sample_on_segment(current, end, x, y)) {
                return true;
            }

            GeomVec2 from = geom_vec2_new(current.x - x, current.y - y);
            GeomVec2 to = geom_vec2_new(end.x - x, end.y - y);
            long double cross =
                (long double)from.x * (long double)to.y
                - (long double)from.y * (long double)to.x;
            long double dot =
                (long double)from.x * (long double)to.x
                + (long double)from.y * (long double)to.y;
            total_angle += atan2l(cross, dot);

            current = end;
            has_line = true;
        }

        if (has_line && !geom_vec2_equal_eps(current, start, 1e-9)) {
            if (dcel_sample_on_segment(current, start, x, y)) {
                return true;
            }

            GeomVec2 from = geom_vec2_new(current.x - x, current.y - y);
            GeomVec2 to = geom_vec2_new(start.x - x, start.y - y);
            long double cross =
                (long double)from.x * (long double)to.y
                - (long double)from.y * (long double)to.x;
            long double dot =
                (long double)from.x * (long double)to.x
                + (long double)from.y * (long double)to.y;
            total_angle += atan2l(cross, dot);
        }
    }

    long long winding = llroundl(total_angle / two_pi);
    long long winding_abs = winding < 0 ? -winding : winding;
    if (fill_rule == DCEL_FILL_RULE_EVEN_ODD) {
        return (winding_abs & 1LL) != 0;
    }
    return winding != 0;
}

static TestResult dcel_test_expect_mask_matches_reference(
    Arena* arena,
    const PathBuilder* path,
    DcelFillRule fill_rule,
    uint32_t width,
    uint32_t height,
    double coordinate_scale
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(path);

    size_t pixel_count = (size_t)width * (size_t)height;
    uint8_t* actual_mask = arena_alloc(arena, pixel_count);
    DcelMaskBounds actual_bounds;
    dcel_rasterize_path_mask(
        arena,
        path,
        fill_rule,
        width,
        height,
        coordinate_scale,
        actual_mask,
        &actual_bounds
    );

    bool has_expected = false;
    DcelMaskBounds expected_bounds = {.is_empty = true,
                                      .min_x = 0,
                                      .min_y = 0,
                                      .max_x = 0,
                                      .max_y = 0};

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            double sample_x = ((double)x + 0.5) / coordinate_scale;
            double sample_y = ((double)y + 0.5) / coordinate_scale;
            bool expected = dcel_test_path_contains_point_reference(
                path,
                fill_rule,
                sample_x,
                sample_y
            );
            size_t idx = (size_t)y * (size_t)width + (size_t)x;
            TEST_ASSERT_EQ(
                (unsigned int)actual_mask[idx],
                expected ? 1U : 0U,
                "Mask mismatch at (%u,%u), sample=(%.8f,%.8f)",
                x,
                y,
                sample_x,
                sample_y
            );

            if (!expected) {
                continue;
            }

            if (!has_expected) {
                expected_bounds = (DcelMaskBounds) {.is_empty = false,
                                                    .min_x = x,
                                                    .min_y = y,
                                                    .max_x = x,
                                                    .max_y = y};
                has_expected = true;
                continue;
            }

            if (x < expected_bounds.min_x) {
                expected_bounds.min_x = x;
            }
            if (y < expected_bounds.min_y) {
                expected_bounds.min_y = y;
            }
            if (x > expected_bounds.max_x) {
                expected_bounds.max_x = x;
            }
            if (y > expected_bounds.max_y) {
                expected_bounds.max_y = y;
            }
        }
    }

    TEST_ASSERT_EQ(actual_bounds.is_empty, (bool)!has_expected);
    if (!has_expected) {
        return TEST_RESULT_PASS;
    }

    TEST_ASSERT_EQ(actual_bounds.min_x, expected_bounds.min_x);
    TEST_ASSERT_EQ(actual_bounds.min_y, expected_bounds.min_y);
    TEST_ASSERT_EQ(actual_bounds.max_x, expected_bounds.max_x);
    TEST_ASSERT_EQ(actual_bounds.max_y, expected_bounds.max_y);

    return TEST_RESULT_PASS;
}

static TestResult dcel_test_expect_point_contains_matches_reference(
    const PathBuilder* path,
    uint32_t width,
    uint32_t height,
    double coordinate_scale
) {
    RELEASE_ASSERT(path);

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            double sample_x = ((double)x + 0.5) / coordinate_scale;
            double sample_y = ((double)y + 0.5) / coordinate_scale;

            for (size_t fill_idx = 0; fill_idx < 2; fill_idx++) {
                DcelFillRule fill_rule = fill_idx == 0
                                           ? DCEL_FILL_RULE_NONZERO
                                           : DCEL_FILL_RULE_EVEN_ODD;
                bool expected = dcel_test_path_contains_point_reference(
                    path,
                    fill_rule,
                    sample_x,
                    sample_y
                );
                bool actual = dcel_path_contains_point(
                    path,
                    fill_rule,
                    sample_x,
                    sample_y
                );
                TEST_ASSERT_EQ(
                    actual,
                    expected,
                    "Point-contains mismatch at (%u,%u), sample=(%.8f,%.8f), fill=%d",
                    x,
                    y,
                    sample_x,
                    sample_y,
                    (int)fill_rule
                );
            }
        }
    }

    return TEST_RESULT_PASS;
}

static TestResult dcel_test_expect_mask_matches_reference_both_fill_rules(
    Arena* arena,
    const PathBuilder* path,
    uint32_t width,
    uint32_t height,
    double coordinate_scale
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(path);

    if (dcel_test_expect_mask_matches_reference(
            arena,
            path,
            DCEL_FILL_RULE_NONZERO,
            width,
            height,
            coordinate_scale
        )
        == TEST_RESULT_FAIL) {
        return TEST_RESULT_FAIL;
    }

    if (dcel_test_expect_mask_matches_reference(
            arena,
            path,
            DCEL_FILL_RULE_EVEN_ODD,
            width,
            height,
            coordinate_scale
        )
        == TEST_RESULT_FAIL) {
        return TEST_RESULT_FAIL;
    }

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_dcel_rasterize_path_mask_monotone_matches_reference_fill_rules) {
    Arena* arena = arena_new(4096);
    PathBuilder* path = path_builder_new(arena);

    dcel_test_add_rect(path, 1.0, 1.0, 9.0, 9.0, false);
    dcel_test_add_rect(path, 3.0, 3.0, 7.0, 7.0, false);

    TestResult result = dcel_test_expect_mask_matches_reference_both_fill_rules(
        arena,
        path,
        12,
        12,
        1.0
    );

    arena_free(arena);
    return result;
}

TEST_FUNC(test_dcel_path_contains_point_winding_orientation_cases) {
    Arena* arena = arena_new(8192);

    // Same orientation nesting: winding 2 in overlap region.
    PathBuilder* same_orientation = path_builder_new(arena);
    dcel_test_add_rect(same_orientation, 1.0, 1.0, 9.0, 9.0, false);
    dcel_test_add_rect(same_orientation, 3.0, 3.0, 7.0, 7.0, false);
    TEST_ASSERT(dcel_path_contains_point(
        same_orientation,
        DCEL_FILL_RULE_NONZERO,
        5.0,
        5.0
    ));
    TEST_ASSERT(!dcel_path_contains_point(
        same_orientation,
        DCEL_FILL_RULE_EVEN_ODD,
        5.0,
        5.0
    ));

    // Opposite orientation nesting: inner cancels winding.
    PathBuilder* opposite_orientation = path_builder_new(arena);
    dcel_test_add_rect(opposite_orientation, 1.0, 1.0, 9.0, 9.0, false);
    dcel_test_add_rect(opposite_orientation, 3.0, 3.0, 7.0, 7.0, true);
    TEST_ASSERT(!dcel_path_contains_point(
        opposite_orientation,
        DCEL_FILL_RULE_NONZERO,
        5.0,
        5.0
    ));
    TEST_ASSERT(!dcel_path_contains_point(
        opposite_orientation,
        DCEL_FILL_RULE_EVEN_ODD,
        5.0,
        5.0
    ));

    // Partial overlap, same orientation: nonzero differs from even-odd in overlap.
    PathBuilder* overlap = path_builder_new(arena);
    dcel_test_add_rect(overlap, 1.0, 1.0, 7.0, 7.0, false);
    dcel_test_add_rect(overlap, 4.0, 2.0, 10.0, 8.0, false);
    TEST_ASSERT(dcel_path_contains_point(
        overlap,
        DCEL_FILL_RULE_NONZERO,
        5.5,
        4.0
    ));
    TEST_ASSERT(!dcel_path_contains_point(
        overlap,
        DCEL_FILL_RULE_EVEN_ODD,
        5.5,
        4.0
    ));

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_dcel_path_contains_point_matches_angle_reference_random_multicontour) {
    uint32_t seed = 0x1337f00du;

    for (size_t case_idx = 0; case_idx < 24; case_idx++) {
        Arena* arena = arena_new(65536);
        PathBuilder* path = path_builder_new(arena);

        size_t contour_count = 2 + (size_t)(dcel_test_lcg_next(&seed) % 3u);
        for (size_t contour_idx = 0; contour_idx < contour_count; contour_idx++) {
            size_t point_count = 5 + (size_t)(dcel_test_lcg_next(&seed) % 6u);
            GeomVec2 points[10];

            double rotation = 2.0 * M_PI * dcel_test_lcg_unit(&seed);
            double center_x = 6.4 + (dcel_test_lcg_unit(&seed) - 0.5) * 4.0;
            double center_y = 6.4 + (dcel_test_lcg_unit(&seed) - 0.5) * 4.0;
            double radius_base = 1.0 + dcel_test_lcg_unit(&seed) * 2.8;
            bool clockwise = (dcel_test_lcg_next(&seed) & 1u) != 0;

            for (size_t idx = 0; idx < point_count; idx++) {
                double jitter = (dcel_test_lcg_unit(&seed) - 0.5) * 0.2;
                double t = ((double)idx + jitter) / (double)point_count;
                double angle = rotation + 2.0 * M_PI * t;
                double radius =
                    radius_base * (0.65 + 0.35 * dcel_test_lcg_unit(&seed));

                points[idx] = geom_vec2_new(
                    center_x + cos(angle) * radius,
                    center_y + sin(angle) * radius
                );
            }

            if (clockwise) {
                for (size_t idx = 0; idx < point_count / 2; idx++) {
                    GeomVec2 tmp = points[idx];
                    points[idx] = points[point_count - idx - 1];
                    points[point_count - idx - 1] = tmp;
                }
            }

            dcel_test_add_polygon(path, points, point_count);
        }

        if (dcel_test_expect_point_contains_matches_reference(path, 32, 32, 3.0)
            == TEST_RESULT_FAIL) {
            arena_free(arena);
            return TEST_RESULT_FAIL;
        }

        arena_free(arena);
    }

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_dcel_rasterize_path_mask_monotone_canvas_edge_clip_cases) {
    Arena* arena = arena_new(8192);

    // Full-canvas rectangle: catches right/bottom clipping off-by-one.
    PathBuilder* full_canvas = path_builder_new(arena);
    dcel_test_add_rect(full_canvas, 0.0, 0.0, 8.0, 8.0, false);
    if (dcel_test_expect_mask_matches_reference(
            arena,
            full_canvas,
            DCEL_FILL_RULE_NONZERO,
            40,
            40,
            5.0
        )
        == TEST_RESULT_FAIL) {
        arena_free(arena);
        return TEST_RESULT_FAIL;
    }

    // Partially out-of-bounds contours on all sides.
    PathBuilder* out_of_bounds = path_builder_new(arena);
    dcel_test_add_rect(out_of_bounds, -2.0, -1.0, 3.7, 4.6, false);
    dcel_test_add_rect(out_of_bounds, 5.2, 5.3, 11.5, 10.9, false);
    if (dcel_test_expect_mask_matches_reference_both_fill_rules(
            arena,
            out_of_bounds,
            40,
            40,
            5.0
        )
        == TEST_RESULT_FAIL) {
        arena_free(arena);
        return TEST_RESULT_FAIL;
    }

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_dcel_rasterize_path_mask_monotone_shared_edge_and_touch_cases) {
    Arena* arena = arena_new(8192);

    // Two contours sharing a full vertical edge.
    PathBuilder* shared_edge = path_builder_new(arena);
    dcel_test_add_rect(shared_edge, 2.0, 2.0, 6.0, 10.0, false);
    dcel_test_add_rect(shared_edge, 6.0, 2.0, 10.0, 10.0, true);
    if (dcel_test_expect_mask_matches_reference_both_fill_rules(
            arena,
            shared_edge,
            72,
            72,
            6.0
        )
        == TEST_RESULT_FAIL) {
        arena_free(arena);
        return TEST_RESULT_FAIL;
    }

    // Two contours touching at one vertex only.
    PathBuilder* touch_vertex = path_builder_new(arena);
    GeomVec2 tri_a[] = {geom_vec2_new(2.0, 2.0),
                        geom_vec2_new(7.0, 2.0),
                        geom_vec2_new(4.5, 6.0)};
    GeomVec2 tri_b[] = {geom_vec2_new(4.5, 6.0),
                        geom_vec2_new(7.0, 10.0),
                        geom_vec2_new(2.0, 10.0)};
    dcel_test_add_polygon(touch_vertex, tri_a, sizeof(tri_a) / sizeof(tri_a[0]));
    dcel_test_add_polygon(touch_vertex, tri_b, sizeof(tri_b) / sizeof(tri_b[0]));
    if (dcel_test_expect_mask_matches_reference_both_fill_rules(
            arena,
            touch_vertex,
            72,
            72,
            6.0
        )
        == TEST_RESULT_FAIL) {
        arena_free(arena);
        return TEST_RESULT_FAIL;
    }

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_dcel_rasterize_path_mask_monotone_matches_reference_concave) {
    Arena* arena = arena_new(4096);
    PathBuilder* path = path_builder_new(arena);

    path_builder_new_contour(path, geom_vec2_new(1.0, 1.0));
    path_builder_line_to(path, geom_vec2_new(11.0, 1.0));
    path_builder_line_to(path, geom_vec2_new(11.0, 11.0));
    path_builder_line_to(path, geom_vec2_new(6.0, 6.0));
    path_builder_line_to(path, geom_vec2_new(1.0, 11.0));
    path_builder_close_contour(path);

    TestResult result = dcel_test_expect_mask_matches_reference(
        arena,
        path,
        DCEL_FILL_RULE_NONZERO,
        64,
        64,
        5.0
    );

    arena_free(arena);
    return result;
}

TEST_FUNC(test_dcel_rasterize_path_mask_monotone_matches_reference_self_intersection) {
    Arena* arena = arena_new(4096);
    PathBuilder* path = path_builder_new(arena);

    path_builder_new_contour(path, geom_vec2_new(2.0, 2.0));
    path_builder_line_to(path, geom_vec2_new(10.0, 10.0));
    path_builder_line_to(path, geom_vec2_new(2.0, 10.0));
    path_builder_line_to(path, geom_vec2_new(10.0, 2.0));
    path_builder_close_contour(path);

    if (dcel_test_expect_mask_matches_reference_both_fill_rules(
            arena,
            path,
            64,
            64,
            5.0
        )
        == TEST_RESULT_FAIL) {
        arena_free(arena);
        return TEST_RESULT_FAIL;
    }

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_dcel_rasterize_path_mask_monotone_split_merge_cases) {
    Arena* arena = arena_new(8192);

    // Concave polygon with local minima/maxima that force split/merge handling.
    GeomVec2 split_merge_a[] = {
        geom_vec2_new(1.0, 1.0),
        geom_vec2_new(9.0, 1.0),
        geom_vec2_new(9.0, 9.0),
        geom_vec2_new(7.0, 9.0),
        geom_vec2_new(7.0, 3.0),
        geom_vec2_new(3.0, 3.0),
        geom_vec2_new(3.0, 9.0),
        geom_vec2_new(1.0, 9.0),
    };
    PathBuilder* path_a = path_builder_new(arena);
    dcel_test_add_polygon(
        path_a,
        split_merge_a,
        sizeof(split_merge_a) / sizeof(split_merge_a[0])
    );
    if (dcel_test_expect_mask_matches_reference(
            arena,
            path_a,
            DCEL_FILL_RULE_NONZERO,
            80,
            80,
            5.0
        )
        == TEST_RESULT_FAIL) {
        arena_free(arena);
        return TEST_RESULT_FAIL;
    }

    // Flipped counterpart to exercise opposite event ordering.
    GeomVec2 split_merge_b[] = {
        geom_vec2_new(1.0, 9.0),
        geom_vec2_new(9.0, 9.0),
        geom_vec2_new(9.0, 1.0),
        geom_vec2_new(7.0, 1.0),
        geom_vec2_new(7.0, 7.0),
        geom_vec2_new(3.0, 7.0),
        geom_vec2_new(3.0, 1.0),
        geom_vec2_new(1.0, 1.0),
    };
    PathBuilder* path_b = path_builder_new(arena);
    dcel_test_add_polygon(
        path_b,
        split_merge_b,
        sizeof(split_merge_b) / sizeof(split_merge_b[0])
    );
    if (dcel_test_expect_mask_matches_reference(
            arena,
            path_b,
            DCEL_FILL_RULE_NONZERO,
            80,
            80,
            5.0
        )
        == TEST_RESULT_FAIL) {
        arena_free(arena);
        return TEST_RESULT_FAIL;
    }

    arena_free(arena);
    return TEST_RESULT_PASS;
}

TEST_FUNC(
    test_dcel_path_contains_point_matches_reference_quantized_random_multicontour
) {
    uint32_t seed = 0x8badf00du;

    for (size_t case_idx = 0; case_idx < 48; case_idx++) {
        Arena* arena = arena_new(65536);
        PathBuilder* path = path_builder_new(arena);

        size_t contour_count = 2 + (size_t)(dcel_test_lcg_next(&seed) % 3u);
        for (size_t contour_idx = 0; contour_idx < contour_count; contour_idx++) {
            size_t point_count = 5 + (size_t)(dcel_test_lcg_next(&seed) % 6u);
            GeomVec2 points[10];

            double rotation = 2.0 * M_PI * dcel_test_lcg_unit(&seed);
            double center_x = 6.4 + (dcel_test_lcg_unit(&seed) - 0.5) * 5.0;
            double center_y = 6.4 + (dcel_test_lcg_unit(&seed) - 0.5) * 5.0;
            double radius_base = 1.0 + dcel_test_lcg_unit(&seed) * 2.8;
            bool clockwise = (dcel_test_lcg_next(&seed) & 1u) != 0;

            for (size_t idx = 0; idx < point_count; idx++) {
                double jitter = (dcel_test_lcg_unit(&seed) - 0.5) * 0.2;
                double t = ((double)idx + jitter) / (double)point_count;
                double angle = rotation + 2.0 * M_PI * t;
                double radius =
                    radius_base * (0.65 + 0.35 * dcel_test_lcg_unit(&seed));
                double px = center_x + cos(angle) * radius;
                double py = center_y + sin(angle) * radius;

                points[idx] = geom_vec2_new(
                    dcel_test_quantize(px, 0.25),
                    dcel_test_quantize(py, 0.25)
                );
            }

            if (clockwise) {
                for (size_t idx = 0; idx < point_count / 2; idx++) {
                    GeomVec2 tmp = points[idx];
                    points[idx] = points[point_count - idx - 1];
                    points[point_count - idx - 1] = tmp;
                }
            }

            dcel_test_add_polygon(path, points, point_count);
        }

        if (dcel_test_expect_point_contains_matches_reference(path, 40, 40, 4.0)
            == TEST_RESULT_FAIL) {
            arena_free(arena);
            return TEST_RESULT_FAIL;
        }

        arena_free(arena);
    }

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_dcel_rasterize_path_mask_monotone_quantized_random_multicontour) {
    uint32_t seed = 0x4a6f7921u;

    for (size_t case_idx = 0; case_idx < 96; case_idx++) {
        Arena* arena = arena_new(65536);
        PathBuilder* path = path_builder_new(arena);

        size_t contour_count = 2 + (size_t)(dcel_test_lcg_next(&seed) % 3u);
        for (size_t contour_idx = 0; contour_idx < contour_count; contour_idx++) {
            size_t point_count = 5 + (size_t)(dcel_test_lcg_next(&seed) % 6u);
            GeomVec2 points[10];

            double rotation = 2.0 * M_PI * dcel_test_lcg_unit(&seed);
            double center_x = 6.4 + (dcel_test_lcg_unit(&seed) - 0.5) * 4.2;
            double center_y = 6.4 + (dcel_test_lcg_unit(&seed) - 0.5) * 4.2;
            double radius_base = 1.0 + dcel_test_lcg_unit(&seed) * 3.0;
            bool clockwise = (dcel_test_lcg_next(&seed) & 1u) != 0;

            for (size_t idx = 0; idx < point_count; idx++) {
                double jitter = (dcel_test_lcg_unit(&seed) - 0.5) * 0.2;
                double t = ((double)idx + jitter) / (double)point_count;
                double angle = rotation + 2.0 * M_PI * t;
                double radius =
                    radius_base * (0.65 + 0.35 * dcel_test_lcg_unit(&seed));
                double px = center_x + cos(angle) * radius;
                double py = center_y + sin(angle) * radius;

                points[idx] = geom_vec2_new(
                    dcel_test_quantize(px, 0.25),
                    dcel_test_quantize(py, 0.25)
                );
            }

            if (clockwise) {
                for (size_t idx = 0; idx < point_count / 2; idx++) {
                    GeomVec2 tmp = points[idx];
                    points[idx] = points[point_count - idx - 1];
                    points[point_count - idx - 1] = tmp;
                }
            }

            dcel_test_add_polygon(path, points, point_count);
        }

        if (dcel_test_expect_mask_matches_reference_both_fill_rules(
                arena,
                path,
                64,
                64,
                5.0
            )
            == TEST_RESULT_FAIL) {
            arena_free(arena);
            return TEST_RESULT_FAIL;
        }

        arena_free(arena);
    }

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_dcel_rasterize_path_mask_monotone_random_simple_polygons) {
    uint32_t seed = 0x94a2f31du;

    for (size_t case_idx = 0; case_idx < 96; case_idx++) {
        Arena* arena = arena_new(32768);
        PathBuilder* path = path_builder_new(arena);

        size_t point_count = 5 + (size_t)(dcel_test_lcg_next(&seed) % 8u);
        GeomVec2 points[12];

        double rotation = 2.0 * M_PI * dcel_test_lcg_unit(&seed);
        double center_x = 6.4 + (dcel_test_lcg_unit(&seed) - 0.5) * 2.0;
        double center_y = 6.4 + (dcel_test_lcg_unit(&seed) - 0.5) * 2.0;
        double radius_base = 1.8 + dcel_test_lcg_unit(&seed) * 2.6;

        for (size_t idx = 0; idx < point_count; idx++) {
            double jitter = (dcel_test_lcg_unit(&seed) - 0.5) * 0.3;
            double t = ((double)idx + jitter) / (double)point_count;
            double angle = rotation + 2.0 * M_PI * t;
            double radius = radius_base * (0.6 + 0.4 * dcel_test_lcg_unit(&seed));

            points[idx] = geom_vec2_new(
                center_x + cos(angle) * radius,
                center_y + sin(angle) * radius
            );
        }

        dcel_test_add_polygon(path, points, point_count);

        if (dcel_test_expect_mask_matches_reference_both_fill_rules(
                arena,
                path,
                64,
                64,
                5.0
            )
            == TEST_RESULT_FAIL) {
            arena_free(arena);
            return TEST_RESULT_FAIL;
        }

        arena_free(arena);
    }

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_dcel_rasterize_path_mask_monotone_random_star_polygons) {
    uint32_t seed = 0x5f3759dfu;

    for (size_t case_idx = 0; case_idx < 96; case_idx++) {
        Arena* arena = arena_new(32768);
        PathBuilder* path = path_builder_new(arena);

        size_t point_count = 5 + (size_t)(dcel_test_lcg_next(&seed) % 4u) * 2u;
        GeomVec2 ring_points[11];
        GeomVec2 star_points[11];

        double rotation = 2.0 * M_PI * dcel_test_lcg_unit(&seed);
        double center_x = 6.4 + (dcel_test_lcg_unit(&seed) - 0.5) * 1.6;
        double center_y = 6.4 + (dcel_test_lcg_unit(&seed) - 0.5) * 1.6;
        double radius_outer = 2.6 + dcel_test_lcg_unit(&seed) * 2.2;
        double radius_inner = radius_outer * (0.35 + 0.25 * dcel_test_lcg_unit(&seed));

        for (size_t idx = 0; idx < point_count; idx++) {
            double angle = rotation + 2.0 * M_PI * ((double)idx / (double)point_count);
            double radius = (idx % 2 == 0) ? radius_outer : radius_inner;

            ring_points[idx] = geom_vec2_new(
                center_x + cos(angle) * radius,
                center_y + sin(angle) * radius
            );
        }

        for (size_t idx = 0; idx < point_count; idx++) {
            size_t star_idx = (idx * 2) % point_count;
            star_points[idx] = ring_points[star_idx];
        }

        dcel_test_add_polygon(path, star_points, point_count);

        if (dcel_test_expect_mask_matches_reference_both_fill_rules(
                arena,
                path,
                64,
                64,
                5.0
            )
            == TEST_RESULT_FAIL) {
            arena_free(arena);
            return TEST_RESULT_FAIL;
        }

        arena_free(arena);
    }

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_dcel_rasterize_path_mask_monotone_random_multicontour) {
    uint32_t seed = 0xcafebabeu;

    for (size_t case_idx = 0; case_idx < 64; case_idx++) {
        Arena* arena = arena_new(65536);
        PathBuilder* path = path_builder_new(arena);

        size_t contour_count = 2 + (size_t)(dcel_test_lcg_next(&seed) % 3u);
        for (size_t contour_idx = 0; contour_idx < contour_count; contour_idx++) {
            size_t point_count = 5 + (size_t)(dcel_test_lcg_next(&seed) % 6u);
            GeomVec2 points[10];

            double rotation = 2.0 * M_PI * dcel_test_lcg_unit(&seed);
            double center_x = 6.4 + (dcel_test_lcg_unit(&seed) - 0.5) * 4.0;
            double center_y = 6.4 + (dcel_test_lcg_unit(&seed) - 0.5) * 4.0;
            double radius_base = 1.0 + dcel_test_lcg_unit(&seed) * 2.8;
            bool clockwise = (dcel_test_lcg_next(&seed) & 1u) != 0;

            for (size_t idx = 0; idx < point_count; idx++) {
                double jitter = (dcel_test_lcg_unit(&seed) - 0.5) * 0.2;
                double t = ((double)idx + jitter) / (double)point_count;
                double angle = rotation + 2.0 * M_PI * t;
                double radius =
                    radius_base * (0.65 + 0.35 * dcel_test_lcg_unit(&seed));

                points[idx] = geom_vec2_new(
                    center_x + cos(angle) * radius,
                    center_y + sin(angle) * radius
                );
            }

            if (clockwise) {
                for (size_t idx = 0; idx < point_count / 2; idx++) {
                    GeomVec2 tmp = points[idx];
                    points[idx] = points[point_count - idx - 1];
                    points[point_count - idx - 1] = tmp;
                }
            }

            dcel_test_add_polygon(path, points, point_count);
        }

        if (dcel_test_expect_mask_matches_reference_both_fill_rules(
                arena,
                path,
                64,
                64,
                5.0
            )
            == TEST_RESULT_FAIL) {
            arena_free(arena);
            return TEST_RESULT_FAIL;
        }

        arena_free(arena);
    }

    return TEST_RESULT_PASS;
}

#endif
