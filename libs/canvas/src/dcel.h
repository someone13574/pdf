#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "arena/arena.h"

typedef struct RasterCanvas RasterCanvas;
typedef struct PathBuilder PathBuilder;

typedef struct DcelVertex DcelVertex;
typedef struct DcelHalfEdge DcelHalfEdge;
typedef struct DcelFace DcelFace;

struct DcelVertex {
    double x;
    double y;
    DcelHalfEdge* incident_edge;
    bool merge;
    bool split;
};

#define DVEC_NAME DcelVertices
#define DVEC_LOWERCASE_NAME dcel_vertices
#define DVEC_TYPE DcelVertex
#include "arena/dvec_decl.h"

struct DcelHalfEdge {
    DcelVertex* origin;
    DcelHalfEdge* twin;
    DcelHalfEdge* next;
    DcelHalfEdge* prev;
    DcelFace* face;
    bool rendered;
};

#define DVEC_NAME DcelHalfEdges
#define DVEC_LOWERCASE_NAME dcel_half_edges
#define DVEC_TYPE DcelHalfEdge
#include "arena/dvec_decl.h"

#define DVEC_NAME DcelFaces
#define DVEC_LOWERCASE_NAME dcel_faces
#define DVEC_TYPE DcelFace*
#include "arena/dvec_decl.h"

struct DcelFace {
    DcelHalfEdge* outer_edge;
    DcelFaces* inner_faces;
};

#define DLINKED_NAME DcelEventQueue
#define DLINKED_LOWERCASE_NAME dcel_event_queue
#define DLINKED_TYPE DcelVertex*
#include "arena/dlinked_decl.h"

typedef struct {
    Arena* arena;

    DcelVertices* vertices;
    DcelHalfEdges* half_edges;
    DcelFaces* faces;

    DcelEventQueue* event_queue;
    DcelFace* outer_face;
} Dcel;

typedef enum { DCEL_FILL_RULE_NONZERO, DCEL_FILL_RULE_EVEN_ODD } DcelFillRule;

typedef struct {
    bool is_empty;
    uint32_t min_x;
    uint32_t min_y;
    uint32_t max_x;
    uint32_t max_y;
} DcelMaskBounds;

Dcel* dcel_new(Arena* arena);

DcelVertex* dcel_add_vertex(Dcel* dcel, double x, double y);
DcelHalfEdge* dcel_add_edge(Dcel* dcel, DcelVertex* a, DcelVertex* b);

DcelVertex* dcel_intersect_edges(
    Dcel* dcel,
    DcelHalfEdge* a,
    DcelHalfEdge* b,
    double intersection_x,
    double intersection_y
);
void dcel_connect_vertices(Dcel* dcel, DcelVertex* a, DcelVertex* b);
DcelHalfEdge* dcel_next_incident_edge(const DcelHalfEdge* half_edge);

void dcel_overlay(Dcel* dcel);
void dcel_assign_faces(Dcel* dcel);
void dcel_partition(Dcel* dcel);

void dcel_render(const Dcel* dcel, RasterCanvas* canvas);

bool dcel_path_contains_point(
    const PathBuilder* path,
    DcelFillRule fill_rule,
    double x,
    double y
);

void dcel_rasterize_path_mask(
    Arena* arena,
    const PathBuilder* path,
    DcelFillRule fill_rule,
    uint32_t width,
    uint32_t height,
    double coordinate_scale,
    uint8_t* out_mask,
    DcelMaskBounds* out_bounds
);
