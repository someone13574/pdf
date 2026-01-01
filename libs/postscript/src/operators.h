#pragma once

#include "arena/arena.h"
#include "pdf_error/error.h"
#include "postscript/interpreter.h"
#include "postscript/object.h"

/// Get a dictionary containing builtin operators.
PostscriptObject postscript_systemdict_ops(Arena* arena);

PdfError* postscript_op_pop(PostscriptInterpreter* interpreter);
PdfError* postscript_op_exch(PostscriptInterpreter* interpreter);
PdfError* postscript_op_dup(PostscriptInterpreter* interpreter);
PdfError* postscript_op_copy(PostscriptInterpreter* interpreter);
PdfError* postscript_op_index(PostscriptInterpreter* interpreter);
PdfError* postscript_op_roll(PostscriptInterpreter* interpreter);

PdfError* postscript_op_add(PostscriptInterpreter* interpreter);
PdfError* postscript_op_sub(PostscriptInterpreter* interpreter);
PdfError* postscript_op_mul(PostscriptInterpreter* interpreter);
PdfError* postscript_op_div(PostscriptInterpreter* interpreter);
PdfError* postscript_op_idiv(PostscriptInterpreter* interpreter);
PdfError* postscript_op_mod(PostscriptInterpreter* interpreter);
PdfError* postscript_op_neg(PostscriptInterpreter* interpreter);
PdfError* postscript_op_abs(PostscriptInterpreter* interpreter);
PdfError* postscript_op_ceiling(PostscriptInterpreter* interpreter);
PdfError* postscript_op_floor(PostscriptInterpreter* interpreter);
PdfError* postscript_op_round(PostscriptInterpreter* interpreter);
PdfError* postscript_op_truncate(PostscriptInterpreter* interpreter);
PdfError* postscript_op_sqrt(PostscriptInterpreter* interpreter);
PdfError* postscript_op_sin(PostscriptInterpreter* interpreter);
PdfError* postscript_op_cos(PostscriptInterpreter* interpreter);
PdfError* postscript_op_atan(PostscriptInterpreter* interpreter);
PdfError* postscript_op_exp(PostscriptInterpreter* interpreter);
PdfError* postscript_op_ln(PostscriptInterpreter* interpreter);
PdfError* postscript_op_log(PostscriptInterpreter* interpreter);
PdfError* postscript_op_cvi(PostscriptInterpreter* interpreter);
PdfError* postscript_op_cvr(PostscriptInterpreter* interpreter);

PdfError* postscript_op_dict(PostscriptInterpreter* interpreter);
PdfError* postscript_op_def(PostscriptInterpreter* interpreter);
PdfError* postscript_op_begin(PostscriptInterpreter* interpreter);
PdfError* postscript_op_end(PostscriptInterpreter* interpreter);
PdfError* postscript_op_currentdict(PostscriptInterpreter* interpreter);

PdfError* postscript_op_defineresource(PostscriptInterpreter* interpreter);
PdfError* postscript_op_findresource(PostscriptInterpreter* interpreter);
