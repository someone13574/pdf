#include "canvas/path_builder.h"

#include "arena/arena.h"
#include "canvas/canvas.h"
#include "dcel.h"
#include "logger/log.h"

#define DVEC_NAME DcelVec
#define DVEC_LOWERCASE_NAME dcel_vec
#define DVEC_TYPE Dcel*
#include "arena/dvec_impl.h"

struct PathBuilder {
    Arena* arena;
    DcelVec* contours;

    Dcel* contour;
    DcelVertex* prev_vertex;
    DcelHalfEdge* first_edge;
    DcelHalfEdge* prev_edge;
};

PathBuilder* path_builder_new(Arena* arena) {
    RELEASE_ASSERT(arena);

    PathBuilder* builder = arena_alloc(arena, sizeof(PathBuilder));
    builder->arena = arena;
    builder->contours = dcel_vec_new(arena);
    builder->contour = NULL;
    builder->prev_vertex = NULL;
    builder->first_edge = NULL;
    builder->prev_edge = NULL;

    return builder;
}

void path_builder_new_contour(PathBuilder* builder, double x, double y) {
    RELEASE_ASSERT(builder);

    if (builder->contour) {
        path_builder_end_contour(builder);
    }

    builder->contour = dcel_new(builder->arena);
    builder->prev_vertex = dcel_add_vertex(builder->contour, x, y);
}

void path_builder_line_to(PathBuilder* builder, double x, double y) {
    RELEASE_ASSERT(builder);
    RELEASE_ASSERT(builder->contour, "No active contour");

    DcelVertex* new_vertex = dcel_add_vertex(builder->contour, x, y);
    DcelHalfEdge* half_edge =
        dcel_add_edge(builder->contour, builder->prev_vertex, new_vertex);

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

    if (!builder->contour) {
        return;
    }

    DcelHalfEdge* closing_edge = dcel_add_edge(
        builder->contour,
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

    dcel_vec_push(builder->contours, builder->contour);

    builder->contour = NULL;
    builder->prev_vertex = NULL;
    builder->first_edge = NULL;
    builder->prev_edge = NULL;
}

Canvas*
path_builder_render(PathBuilder* builder, uint32_t width, uint32_t height) {
    Canvas* canvas = canvas_new(builder->arena, width, height, 0xffffffff);

    if (dcel_vec_len(builder->contours) == 0) {
        return canvas;
    }

    Dcel* dcel;
    RELEASE_ASSERT(dcel_vec_get(builder->contours, 0, &dcel));

    for (size_t contour_idx = 1; contour_idx < dcel_vec_len(builder->contours);
         contour_idx++) {
        Dcel* contour;
        RELEASE_ASSERT(dcel_vec_get(builder->contours, contour_idx, &contour));

        dcel_overlay(dcel, contour);
    }

    return canvas;
}
