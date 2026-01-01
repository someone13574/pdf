#pragma once

#include <stdint.h>

#include "arena/arena.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"
#include "postscript/object.h"

typedef struct {
    /// (Required) The function type:
    /// 0 - Sampled function
    /// 2 - Exponential interpolation function
    /// 3 - Stitching function
    /// 4 - PostScript calculator function
    PdfInteger type;

    /// (Required) An array of 2 × m numbers, where m shall be the number of
    /// input values. For each i from 0 to m − 1, Domain2i shall be less than or
    /// equal to Domain2i+1 , and the ith input value, xi , shall lie in the
    /// interval Domain2i ≤ xi ≤ Domain2i+1 . Input values outside the declared
    /// domain shall be clipped to the nearest boundary value.
    PdfNumberVec* domain;

    /// (Required for type 0 and type 4 functions, optional otherwise; see
    /// below) An array of 2 × n numbers, where n shall be the number of output
    /// values. For each j from 0 to n − 1, Range2j shall be less than or equal
    /// to Range2j+1 , and the jth output value, yj , shall lie in the interval
    /// Range2j ≤ yj ≤ Range2j+1. Output values outside the declared range shall
    /// be clipped to the nearest boundary value. If this entry is absent, no
    /// clipping shall be done.
    PdfNumberVecOptional range;

    union {
        PostscriptInterpreter* type4;
    } data;
} PdfFunction;

PdfError* pdf_deserialize_function(
    const PdfObject* object,
    PdfFunction* target_ptr,
    PdfResolver* resolver
);

/// Run a function using the operands in io and returning the outputs in io
PdfError*
pdf_run_function(const PdfFunction* function, Arena* arena, PdfObjectVec* io);
