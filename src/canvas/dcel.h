#pragma once

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
    DcelFace* incident_face;
};

#define DVEC_NAME DcelHalfEdges
#define DVEC_LOWERCASE_NAME dcel_half_edges
#define DVEC_TYPE DcelHalfEdge
#include "arena/dvec_decl.h"

struct DcelFace {
    DcelHalfEdge* outer_component;
    DcelHalfEdges* inner_components;
};

#define DVEC_NAME DcelFaces
#define DVEC_LOWERCASE_NAME dcel_faces
#define DVEC_TYPE DcelFace
#include "arena/dvec_decl.h"

typedef struct {
    Arena* arena;
    DcelVertices* vertices;
    DcelHalfEdges* half_edges;
    DcelFaces* faces;
} Dcel;

Dcel* dcel_new(Arena* arena);

DcelVertex* dcel_add_vertex(Dcel* dcel, double x, double y);
DcelHalfEdge* dcel_add_edge(Dcel* dcel, DcelVertex* a, DcelVertex* b);
DcelFace* dcel_add_face(Dcel* dcel, DcelHalfEdge* outer);

Dcel* dcel_overlay(Dcel* a, Dcel* b);

void dcel_render(Dcel* dcel, Canvas* canvas);
