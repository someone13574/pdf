#pragma once

#include "arena/arena.h"
#include "postscript/interpreter.h"
#include "postscript/object.h"

/// Get a dictionary containing builtin procedures.
PostscriptObject postscript_systemdict_procedures(Arena* arena);

PdfError* postscript_proc_begin(PostscriptInterpreter* interpreter);
PdfError* postscript_proc_findresource(PostscriptInterpreter* interpreter);
