#pragma once

#include "arena/arena.h"
#include "pdf_error/error.h"
#include "postscript/interpreter.h"
#include "postscript/object.h"

/// Get a dictionary containing builtin operators.
PostscriptObject postscript_systemdict_ops(Arena* arena);

PdfError* postscript_op_dup(PostscriptInterpreter* interpreter);

PdfError* postscript_op_dict(PostscriptInterpreter* interpreter);
PdfError* postscript_op_def(PostscriptInterpreter* interpreter);
PdfError* postscript_op_begin(PostscriptInterpreter* interpreter);
PdfError* postscript_op_end(PostscriptInterpreter* interpreter);
PdfError* postscript_op_currentdict(PostscriptInterpreter* interpreter);

PdfError* postscript_op_defineresource(PostscriptInterpreter* interpreter);
PdfError* postscript_op_findresource(PostscriptInterpreter* interpreter);
