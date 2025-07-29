#include "dcel.h"

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

#define DVEC_NAME DcelFaces
#define DVEC_LOWERCASE_NAME dcel_faces
#define DVEC_TYPE DcelFace
#include "arena/dvec_impl.h"

Dcel* dcel_new(Arena* arena) {
    RELEASE_ASSERT(arena);

    Dcel* dcel = arena_alloc(arena, sizeof(Dcel));
    dcel->arena = arena;
    dcel->vertices = dcel_vertices_new(arena);
    dcel->half_edges = dcel_half_edges_new(arena);
    dcel->faces = dcel_faces_new(arena);

    return dcel;
}

DcelVertex* dcel_add_vertex(Dcel* dcel, double x, double y) {
    RELEASE_ASSERT(dcel);

    DcelVertex vertex = {.x = x, .y = y, .incident_edge = NULL};
    return dcel_vertices_push(dcel->vertices, vertex);
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
           .incident_face = NULL}
    );

    DcelHalfEdge* half_edge_b = dcel_half_edges_push(
        dcel->half_edges,
        (DcelHalfEdge
        ) {.origin = b,
           .twin = NULL,
           .next = NULL,
           .prev = NULL,
           .incident_face = NULL}
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

DcelFace* dcel_add_face(Dcel* dcel, DcelHalfEdge* outer_component) {
    RELEASE_ASSERT(dcel);
    RELEASE_ASSERT(outer_component);

    DcelFace* face = dcel_faces_push(
        dcel->faces,
        (DcelFace) {.outer_component = outer_component}
    );

    DcelHalfEdge* half_edge = outer_component;
    do {
        half_edge->incident_face = face;
        half_edge = half_edge->next;
    } while (half_edge != outer_component);

    return face;
}

void dcel_render(Dcel* dcel, Canvas* canvas) {
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
            0x000000ff
        );
    }

    for (size_t vertex_idx = 0; vertex_idx < dcel_vertices_len(dcel->vertices);
         vertex_idx++) {
        DcelVertex vertex;
        RELEASE_ASSERT(dcel_vertices_get(dcel->vertices, vertex_idx, &vertex));

        canvas_draw_circle(canvas, vertex.x, vertex.y, 10.0, 0x000000ff);
    }
}
