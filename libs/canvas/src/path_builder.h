#pragma once

#include "geom/vec2.h"

typedef struct {
    GeomVec2 control;
    GeomVec2 end;
} PathQuadBezier;

typedef struct {
    GeomVec2 control_a;
    GeomVec2 control_b;
    GeomVec2 end;
} PathCubicBezier;

typedef struct {
    enum {
        PATH_CONTOUR_SEGMENT_TYPE_START,
        PATH_CONTOUR_SEGMENT_TYPE_LINE,
        PATH_CONTOUR_SEGMENT_TYPE_QUAD_BEZIER,
        PATH_CONTOUR_SEGMENT_TYPE_CUBIC_BEZIER
    } type;

    union {
        GeomVec2 start;
        GeomVec2 line;
        PathQuadBezier quad_bezier;
        PathCubicBezier cubic_bezier;
    } value;
} PathContourSegment;

GeomVec2 path_contour_segment_end(PathContourSegment segment);

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
