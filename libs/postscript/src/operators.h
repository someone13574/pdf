#pragma once

#include "arena/arena.h"
#include "pdf_error/error.h"
#include "postscript/interpreter.h"
#include "postscript/object.h"

/// Get a dictionary containing builtin operators.
PSObject ps_systemdict_ops(Arena* arena);

PdfError* ps_builtin_op_pop(PSInterpreter* interpreter);
PdfError* ps_builtin_op_exch(PSInterpreter* interpreter);
PdfError* ps_builtin_op_dup(PSInterpreter* interpreter);
PdfError* ps_builtin_op_copy(PSInterpreter* interpreter);
PdfError* ps_builtin_op_index(PSInterpreter* interpreter);
PdfError* ps_builtin_op_roll(PSInterpreter* interpreter);

PdfError* ps_builtin_op_add(PSInterpreter* interpreter);
PdfError* ps_builtin_op_sub(PSInterpreter* interpreter);
PdfError* ps_builtin_op_mul(PSInterpreter* interpreter);
PdfError* ps_builtin_op_div(PSInterpreter* interpreter);
PdfError* ps_builtin_op_idiv(PSInterpreter* interpreter);
PdfError* ps_builtin_op_mod(PSInterpreter* interpreter);
PdfError* ps_builtin_op_neg(PSInterpreter* interpreter);
PdfError* ps_builtin_op_abs(PSInterpreter* interpreter);
PdfError* ps_builtin_op_ceiling(PSInterpreter* interpreter);
PdfError* ps_builtin_op_floor(PSInterpreter* interpreter);
PdfError* ps_builtin_op_round(PSInterpreter* interpreter);
PdfError* ps_builtin_op_truncate(PSInterpreter* interpreter);
PdfError* ps_builtin_op_sqrt(PSInterpreter* interpreter);
PdfError* ps_builtin_op_sin(PSInterpreter* interpreter);
PdfError* ps_builtin_op_cos(PSInterpreter* interpreter);
PdfError* ps_builtin_op_atan(PSInterpreter* interpreter);
PdfError* ps_builtin_op_exp(PSInterpreter* interpreter);
PdfError* ps_builtin_op_ln(PSInterpreter* interpreter);
PdfError* ps_builtin_op_log(PSInterpreter* interpreter);
PdfError* ps_builtin_op_cvi(PSInterpreter* interpreter);
PdfError* ps_builtin_op_cvr(PSInterpreter* interpreter);

PdfError* ps_builtin_op_dict(PSInterpreter* interpreter);
PdfError* ps_builtin_op_def(PSInterpreter* interpreter);
PdfError* ps_builtin_op_begin(PSInterpreter* interpreter);
PdfError* ps_builtin_op_end(PSInterpreter* interpreter);
PdfError* ps_builtin_op_currentdict(PSInterpreter* interpreter);

PdfError* ps_builtin_op_defineresource(PSInterpreter* interpreter);
PdfError* ps_builtin_op_findresource(PSInterpreter* interpreter);
