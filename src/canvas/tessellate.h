#pragma once

#include "canvas/canvas.h"

typedef enum {
    TESS_POINT_TYPE_REGULAR,
    TESS_POINT_TYPE_START,
    TESS_POINT_TYPE_END,
    TESS_POINT_TYPE_SPLIT,
    TESS_POINT_TYPE_MERGE
} TessPointType;

typedef struct TessPoint {
    double x;
    double y;
    struct TessPoint* next;
    struct TessPoint* prev;
    TessPointType type;
    bool prev_below;
    bool next_below;
} TessPoint;

#define DVEC_NAME TessPointVec
#define DVEC_LOWERCASE_NAME tess_point_vec
#define DVEC_TYPE TessPoint
#include "arena/dvec_decl.h"

#define DLINKED_NAME TessPointQueue
#define DLINKED_LOWERCASE_NAME tess_point_queue
#define DLINKED_TYPE TessPoint*
#include "arena/dlinked_decl.h"

typedef struct {
    TessPointVec* points;
    TessPoint* start;
    TessPoint* end;
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
