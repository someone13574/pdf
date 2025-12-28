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

/// Display the interpreter's current state.
void postscript_interpreter_dump(PostscriptInterpreter* interpreter);

/// Get the interpreter's arena.
Arena* postscript_interpreter_get_arena(PostscriptInterpreter* interpreter);

/// Get the resource categories associated with the interpreter.
PostscriptResourceCategoryVec* postscript_interpreter_get_resource_categories(
    PostscriptInterpreter* interpreter
);

/// Add a new operator to a resource.
void postscript_interpreter_add_operator(
    PostscriptInterpreter* interpreter,
    char* category_name,
    char* resource_name,
    PostscriptOperator operator,
    char * operator_name
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

/// Get the top of the dictionary stack.
void postscript_interpreter_dict(
    PostscriptInterpreter* interpreter,
    PostscriptObject* object_out
);

/// Push a dictionary to the dictionary stack.
void postscript_interpreter_dict_push(
    PostscriptInterpreter* interpreter,
    PostscriptObject dictionary
);

/// Pop a dictionary from the dictionary stack.
PdfError* postscript_interpreter_dict_pop(PostscriptInterpreter* interpreter);

/// Get a dictionary value from the dictionary stack.
PdfError* postscript_interpreter_dict_entry(
    const PostscriptInterpreter* interpreter,
    const PostscriptObject* key,
    PostscriptObject* value_out
);

/// Define a new key-value pair in the current dictionary.
PdfError* postscript_interpreter_define(
    PostscriptInterpreter* interpreter,
    PostscriptObject key,
    PostscriptObject value
);

/// Gets the current user-data.
PdfError* postscript_interpreter_user_data(
    PostscriptInterpreter* interpreter,
    const char* expected_name,
    void** user_data_out
);

/// Pushes a new user-data to the stack.
void postscript_interpreter_user_data_push(
    PostscriptInterpreter* interpreter,
    void* user_data,
    const char* name
);

/// Pops a user-data from the stack.
PdfError* postscript_interpreter_user_data_pop(
    PostscriptInterpreter* interpreter,
    const char* expected_name
);
