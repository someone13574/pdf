#pragma once

#include <stdint.h>

#include "arena/arena.h"
#include "canvas/canvas.h"
typedef struct DcelVertex DcelVertex;
typedef struct DcelHalfEdge DcelHalfEdge;
typedef struct DcelFace DcelFace;

struct DcelVertex {
    double x;
    double y;
    DcelHalfEdge* incident_edge;
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
};

#define DVEC_NAME DcelHalfEdges
#define DVEC_LOWERCASE_NAME dcel_half_edges
#define DVEC_TYPE DcelHalfEdge
#include "arena/dvec_decl.h"

#define DLINKED_NAME DcelEventQueue
#define DLINKED_LOWERCASE_NAME dcel_event_queue
#define DLINKED_TYPE DcelVertex*
#include "arena/dlinked_decl.h"

typedef struct {
    Arena* arena;

    DcelVertices* vertices;
    DcelHalfEdges* half_edges;

    DcelEventQueue* event_queue;
} Dcel;

Dcel* dcel_new(Arena* arena);

DcelVertex* dcel_add_vertex(Dcel* dcel, double x, double y);
DcelHalfEdge* dcel_add_edge(Dcel* dcel, DcelVertex* a, DcelVertex* b);
DcelHalfEdge* dcel_next_incident_edge(DcelHalfEdge* half_edge);

void dcel_overlay(Dcel* dcel, Dcel* overlay);

void dcel_render(Dcel* dcel, uint32_t rgba, Canvas* canvas);
