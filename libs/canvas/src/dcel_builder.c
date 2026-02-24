#include "canvas/dcel_builder.h"

#include "arena/arena.h"
#include "canvas.h"
#include "canvas/canvas.h"
#include "dcel.h"
#include "logger/log.h"

struct DcelBuilder {
    Arena* arena;
    Dcel* dcel;

    DcelVertex* prev_vertex;
    DcelHalfEdge* first_edge;
    DcelHalfEdge* prev_edge;
};

DcelBuilder* dcel_builder_new(Arena* arena) {
    RELEASE_ASSERT(arena);

    DcelBuilder* builder = arena_alloc(arena, sizeof(DcelBuilder));
    builder->arena = arena;
    builder->dcel = dcel_new(arena);
    builder->prev_vertex = NULL;
    builder->first_edge = NULL;
    builder->prev_edge = NULL;

    return builder;
}

void dcel_builder_new_contour(DcelBuilder* builder, double x, double y) {
    RELEASE_ASSERT(builder);

    if (builder->prev_vertex) {
        dcel_builder_end_contour(builder);
    }

    builder->prev_vertex = dcel_add_vertex(builder->dcel, x, y);
}

void dcel_builder_line_to(DcelBuilder* builder, double x, double y) {
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

void dcel_builder_end_contour(DcelBuilder* builder) {
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

Canvas* dcel_builder_render(
    const DcelBuilder* builder,
    uint32_t width,
    uint32_t height
) {
    uint32_t resolution_multiplier = 2;
    RasterCanvas* canvas = raster_canvas_new(
        builder->arena,
        width * resolution_multiplier,
        height * resolution_multiplier,
        rgba_new(1.0, 1.0, 1.0, 1.0),
        (double)resolution_multiplier
    );

    // dcel_overlay(builder->dcel);
    // dcel_assign_faces(builder->dcel);
    // dcel_partition(builder->dcel);
    dcel_render(builder->dcel, canvas);

    return canvas_from_raster(builder->arena, canvas);
}
