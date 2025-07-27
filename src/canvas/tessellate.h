#pragma once

#include "canvas/canvas.h"

typedef struct {
    double x;
    double y;
} TessPoint;

typedef struct {
    double x;
    double y;
    size_t point_idx;
    size_t contour_idx;
} TessQueuePoint;

#define DVEC_NAME TessPointVec
#define DVEC_LOWERCASE_NAME tess_point_vec
#define DVEC_TYPE TessPoint
#include "arena/dvec_decl.h"

#define DLINKED_NAME TessPointQueue
#define DLINKED_LOWERCASE_NAME tess_point_queue
#define DLINKED_TYPE TessQueuePoint
#include "arena/dlinked_decl.h"

typedef struct {
    TessPointVec* points;
} TessContour;

#define DVEC_NAME TessContourVec
#define DVEC_LOWERCASE_NAME tess_contour_vec
#define DVEC_TYPE TessContour
#include "arena/dvec_decl.h"

typedef struct {
    Arena* arena;
    TessContourVec* contours;
    TessPointQueue* point_queue;
} TessPoly;

void tess_poly_new(Arena* arena, TessPoly* poly);

void tess_poly_move_to(TessPoly* poly, double x, double y);
void tess_poly_line_to(TessPoly* poly, double x, double y);

void tess_poly_tessellate(TessPoly* poly);
void tess_poly_render(TessPoly* poly, Canvas* canvas);
