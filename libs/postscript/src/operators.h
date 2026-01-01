#pragma once

#include "arena/arena.h"
#include "err/error.h"
#include "postscript/interpreter.h"
#include "postscript/object.h"

/// Get a dictionary containing builtin operators.
PSObject ps_systemdict_ops(Arena* arena);

Error* ps_builtin_op_pop(PSInterpreter* interpreter);
Error* ps_builtin_op_exch(PSInterpreter* interpreter);
Error* ps_builtin_op_dup(PSInterpreter* interpreter);
Error* ps_builtin_op_copy(PSInterpreter* interpreter);
Error* ps_builtin_op_index(PSInterpreter* interpreter);
Error* ps_builtin_op_roll(PSInterpreter* interpreter);

Error* ps_builtin_op_add(PSInterpreter* interpreter);
Error* ps_builtin_op_sub(PSInterpreter* interpreter);
Error* ps_builtin_op_mul(PSInterpreter* interpreter);
Error* ps_builtin_op_div(PSInterpreter* interpreter);
Error* ps_builtin_op_idiv(PSInterpreter* interpreter);
Error* ps_builtin_op_mod(PSInterpreter* interpreter);
Error* ps_builtin_op_neg(PSInterpreter* interpreter);
Error* ps_builtin_op_abs(PSInterpreter* interpreter);
Error* ps_builtin_op_ceiling(PSInterpreter* interpreter);
Error* ps_builtin_op_floor(PSInterpreter* interpreter);
Error* ps_builtin_op_round(PSInterpreter* interpreter);
Error* ps_builtin_op_truncate(PSInterpreter* interpreter);
Error* ps_builtin_op_sqrt(PSInterpreter* interpreter);
Error* ps_builtin_op_sin(PSInterpreter* interpreter);
Error* ps_builtin_op_cos(PSInterpreter* interpreter);
Error* ps_builtin_op_atan(PSInterpreter* interpreter);
Error* ps_builtin_op_exp(PSInterpreter* interpreter);
Error* ps_builtin_op_ln(PSInterpreter* interpreter);
Error* ps_builtin_op_log(PSInterpreter* interpreter);
Error* ps_builtin_op_cvi(PSInterpreter* interpreter);
Error* ps_builtin_op_cvr(PSInterpreter* interpreter);

Error* ps_builtin_op_dict(PSInterpreter* interpreter);
Error* ps_builtin_op_def(PSInterpreter* interpreter);
Error* ps_builtin_op_begin(PSInterpreter* interpreter);
Error* ps_builtin_op_end(PSInterpreter* interpreter);
Error* ps_builtin_op_currentdict(PSInterpreter* interpreter);

Error* ps_builtin_op_defineresource(PSInterpreter* interpreter);
Error* ps_builtin_op_findresource(PSInterpreter* interpreter);
