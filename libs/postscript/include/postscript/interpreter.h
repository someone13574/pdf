#pragma once

#include <stdbool.h>

#include "arena/arena.h"
#include "pdf_error/error.h"
#include "postscript/object.h"
#include "postscript/resource.h"
#include "postscript/tokenizer.h"

/// A postscript interpreter instance.
typedef struct PostscriptInterpreter PostscriptInterpreter;

/// Create a new postscript interpreter instance.
PostscriptInterpreter* postscript_interpreter_new(Arena* arena);

/// Get the resource categories associated with the interpreter.
PostscriptResourceCategoryVec* postscript_interpreter_get_resource_categories(
    PostscriptInterpreter* interpreter
);

/// Add a new custom procedure to a resource.
void postscript_interpreter_add_proc(
    PostscriptInterpreter* interpreter,
    char* category_name,
    char* resource_name,
    PostscriptCustomProcedure procedure,
    char* procedure_name
);

/// Process a token.
PdfError* postscript_interpret_token(
    PostscriptInterpreter* interpreter,
    PostscriptToken token
);

/// Pop an operand from the top of the operand stack, returning it in
/// `object_out`.
PdfError* postscript_interpreter_pop_operand(
    PostscriptInterpreter* interpreter,
    PostscriptObject* object_out
);

/// Pop an operand from the top of the operand stack, check its type, and return
/// it in `object_out`.
PdfError* postscript_interpreter_pop_operand_typed(
    PostscriptInterpreter* interpreter,
    PostscriptObjectType expected_type,
    bool literal,
    PostscriptObject* object_out
);

/// Push an operand to the operand stack.
void postscript_interpreter_operand_push(
    PostscriptInterpreter* interpreter,
    PostscriptObject object
);

/// Push a dictionary to the dictionary stack.
void postscript_interpreter_dict_push(
    PostscriptInterpreter* interpreter,
    PostscriptObject dictionary
);

/// Get a dictionary value from the dictionary stack.
PdfError* postscript_interpreter_dict_entry(
    const PostscriptInterpreter* interpreter,
    const PostscriptObject* key,
    PostscriptObject* value_out
);
