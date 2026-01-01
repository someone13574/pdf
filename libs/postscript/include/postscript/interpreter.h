#pragma once

#include <stdbool.h>

#include "arena/arena.h"
#include "err/error.h"
#include "postscript/object.h"
#include "postscript/resource.h"
#include "postscript/tokenizer.h"

/// A postscript interpreter instance.
typedef struct PSInterpreter PSInterpreter;

/// Create a new postscript interpreter instance.
PSInterpreter* ps_interpreter_new(Arena* arena);

/// Display the interpreter's current state.
void ps_interpreter_dump(PSInterpreter* interpreter);

/// Get the interpreter's arena.
Arena* ps_interpreter_get_arena(PSInterpreter* interpreter);

/// Get the resource categories associated with the interpreter.
PSResourceCategoryVec*
ps_interpreter_get_resource_categories(PSInterpreter* interpreter);

/// Add a new operator to a resource.
void ps_interpreter_add_operator(
    PSInterpreter* interpreter,
    char* category_name,
    char* resource_name,
    PSOperator operator,
    char * operator_name
);

/// Process a token.
Error* ps_interpret_token(PSInterpreter* interpreter, PSToken token);

/// Interpret all tokens in a stream
Error* ps_interpret_tokens(PSInterpreter* interpreter, PSTokenizer* tokenizer);

/// Process an object
Error* ps_interpret_object(PSInterpreter* interpreter, PSObject object);

/// Get the operand stack of the interpreter
PSObjectList* ps_interpreter_stack(PSInterpreter* interpreter);

/// Pop an operand from the top of the operand stack, returning it in
/// `object_out`.
Error*
ps_interpreter_pop_operand(PSInterpreter* interpreter, PSObject* object_out);

/// Pop an operand from the top of the operand stack, check its type, and return
/// it in `object_out`.
Error* ps_interpreter_pop_operand_typed(
    PSInterpreter* interpreter,
    PSObjectType expected_type,
    bool literal,
    PSObject* object_out
);

/// Push an operand to the operand stack.
void ps_interpreter_operand_push(PSInterpreter* interpreter, PSObject object);

/// Get the top of the dictionary stack.
void ps_interpreter_dict(PSInterpreter* interpreter, PSObject* object_out);

/// Push a dictionary to the dictionary stack.
void ps_interpreter_dict_push(PSInterpreter* interpreter, PSObject dictionary);

/// Pop a dictionary from the dictionary stack.
Error* ps_interpreter_dict_pop(PSInterpreter* interpreter);

/// Get a dictionary value from the dictionary stack.
Error* ps_interpreter_dict_entry(
    const PSInterpreter* interpreter,
    const PSObject* key,
    PSObject* value_out
);

/// Define a new key-value pair in the current dictionary.
Error*
ps_interpreter_define(PSInterpreter* interpreter, PSObject key, PSObject value);

/// Gets the current user-data.
Error* ps_interpreter_user_data(
    PSInterpreter* interpreter,
    const char* expected_name,
    void** user_data_out
);

/// Pushes a new user-data to the stack.
void ps_interpreter_user_data_push(
    PSInterpreter* interpreter,
    void* user_data,
    const char* name
);

/// Pops a user-data from the stack.
Error* ps_interpreter_user_data_pop(
    PSInterpreter* interpreter,
    const char* expected_name
);
