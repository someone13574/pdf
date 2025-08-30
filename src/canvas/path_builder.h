#pragma once

typedef struct {
    double x;
    double y;
} PathPoint;

typedef struct {
    PathPoint control;
    PathPoint end;
} PathQuadBezier;

typedef struct {
    enum {
        PATH_CONTOUR_SEGMENT_TYPE_START,
        PATH_CONTOUR_SEGMENT_TYPE_LINE,
        PATH_CONTOUR_SEGMENT_TYPE_BEZIER
    } type;

    union {
        PathPoint start;
        PathPoint line;
        PathQuadBezier bezier;
    } data;
} PathContourSegment;

#define DVEC_NAME PathContour
#define DVEC_LOWERCASE_NAME path_contour
#define DVEC_TYPE PathContourSegment
#include "arena/dvec_decl.h"

#define DVEC_NAME PathContourVec
#define DVEC_LOWERCASE_NAME path_contour_vec
#define DVEC_TYPE PathContour*
#include "arena/dvec_decl.h"

struct PathBuilder {
    Arena* arena;
    PathContourVec* contours;
};
