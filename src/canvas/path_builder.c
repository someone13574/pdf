#include "canvas/path_builder.h"

#include "arena/arena.h"
#include "canvas/canvas.h"
#include "dcel.h"
#include "logger/log.h"

struct PathBuilder {
    Arena* arena;
    Dcel* dcel;

    DcelVertex* prev_vertex;
    DcelHalfEdge* first_edge;
    DcelHalfEdge* prev_edge;
};

PathBuilder* path_builder_new(Arena* arena) {
    RELEASE_ASSERT(arena);

    PathBuilder* builder = arena_alloc(arena, sizeof(PathBuilder));
    builder->arena = arena;
    builder->dcel = dcel_new(arena);
    builder->prev_vertex = NULL;
    builder->first_edge = NULL;
    builder->prev_edge = NULL;

    return builder;
}

void path_builder_new_contour(PathBuilder* builder, double x, double y) {
    RELEASE_ASSERT(builder);

    if (builder->prev_vertex) {
        path_builder_end_contour(builder);
    }

    builder->prev_vertex = dcel_add_vertex(builder->dcel, x, y);
}

void path_builder_line_to(PathBuilder* builder, double x, double y) {
    RELEASE_ASSERT(builder);
    RELEASE_ASSERT(builder->dcel, "No active contour");

    DcelVertex* new_vertex = dcel_add_vertex(builder->dcel, x, y);
    DcelHalfEdge* half_edge =
        dcel_add_edge(builder->dcel, builder->prev_vertex, new_vertex);

    if (builder->first_edge) {
        builder->prev_edge->next = half_edge;
        half_edge->prev = builder->prev_edge;

        builder->prev_edge->twin->prev = half_edge->twin;
        half_edge->twin->next = builder->prev_edge->twin;
    } else {
        builder->first_edge = half_edge;
    }

    builder->prev_vertex = new_vertex;
    builder->prev_edge = half_edge;
}

void path_builder_end_contour(PathBuilder* builder) {
    RELEASE_ASSERT(builder);

    if (!builder->dcel) {
        return;
    }

    DcelHalfEdge* closing_edge = dcel_add_edge(
        builder->dcel,
        builder->prev_vertex,
        builder->first_edge->origin
    );

    builder->first_edge->prev = closing_edge;
    builder->prev_edge->next = closing_edge;
    closing_edge->next = builder->first_edge;
    closing_edge->prev = builder->prev_edge;

    builder->first_edge->twin->next = closing_edge->twin;
    builder->prev_edge->twin->prev = closing_edge->twin;
    closing_edge->twin->next = builder->prev_edge->twin;
    closing_edge->twin->prev = builder->first_edge->twin;

    builder->prev_vertex = NULL;
    builder->first_edge = NULL;
    builder->prev_edge = NULL;
}

Canvas*
path_builder_render(PathBuilder* builder, uint32_t width, uint32_t height) {
    Canvas* canvas =
        canvas_new(builder->arena, width * 2, height * 2, 0xffffffff, 2.0);

    dcel_overlay(builder->dcel);
    dcel_assign_faces(builder->dcel);
    dcel_render(builder->dcel, 0x000000ff, canvas, NULL);

    return canvas;
}
