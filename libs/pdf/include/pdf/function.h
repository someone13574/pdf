#pragma once

#include <stdint.h>

#include "arena/arena.h"
#include "err/error.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "postscript/object.h"

typedef struct PdfFunction PdfFunction;

#define DVEC_NAME PdfFunctionVec
#define DVEC_LOWERCASE_NAME pdf_function_vec
#define DVEC_TYPE PdfFunction
#include "arena/dvec_decl.h"

typedef struct {
    /// (Optional) An array of n numbers that shall define the function result
    /// when x = 0.0. Default value: [ 0.0 ].
    PdfNumberVecOptional c0;

    /// (Optional) An array of n numbers that shall define the function result
    /// when x = 1.0. Default value: [ 1.0 ].
    PdfNumberVecOptional c1;

    /// (Required) The interpolation exponent. Each input value x shall return n
    /// values, given by yj = C0j + xN × (C1j − C0j ), for 0 ≤ j < n.
    PdfNumber n;
} PdfFunctionType2;

typedef struct {
    /// (Required) An array of k 1-input functions that shall make up the
    /// stitching function. The output dimensionality of all functions shall be
    /// the same, and compatible with the value of Range if Range is present.
    PdfFunctionVec* functions;

    /// (Required) An array of k − 1 numbers that, in combination with Domain,
    /// shall define the intervals to which each function from the Functions
    /// array shall apply. Bounds elements shall be in order of increasing
    /// value, and each value shall be within the domain defined by Domain.
    PdfNumberVec* bounds;

    /// (Required) An array of 2 × k numbers that, taken in pairs, shall map
    /// each subset of the domain defined by Domain and the Bounds array to the
    /// domain of the corresponding function.
    PdfNumberVec* encode;
} PdfFunctionType3;

struct PdfFunction {
    /// (Required) The function type:
    /// 0 - Sampled function
    /// 2 - Exponential interpolation function
    /// 3 - Stitching function
    /// 4 - PostScript calculator function
    PdfInteger function_type;

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
        PdfFunctionType2 type2;
        PdfFunctionType3 type3;
        PSInterpreter* type4;
    } data;
};

Error* pdf_deserialize_function(
    const PdfObject* object,
    PdfFunction* target_ptr,
    PdfResolver* resolver
);

/// Run a function using the operands in io and returning the outputs in io
Error*
pdf_run_function(const PdfFunction* function, Arena* arena, PdfObjectVec* io);

DESERDE_DECL_TRAMPOLINE(pdf_deserialize_function_trampoline)
