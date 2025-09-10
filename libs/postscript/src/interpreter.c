#include "postscript/interpreter.h"

#include <stdbool.h>
#include <string.h>

#include "arena/arena.h"
#include "logger/log.h"
#include "operators.h"
#include "pdf_error/error.h"
#include "postscript/object.h"
#include "postscript/resource.h"
#include "postscript/tokenizer.h"

#define DLINKED_NAME PostscriptListStack
#define DLINKED_LOWERCASE_NAME postscript_list_stack
#define DLINKED_TYPE PostscriptObjectList*
#include "arena/dlinked_impl.h"

typedef struct {
    const char* name;
    void* data;
} PostscriptUserData;

#define DLINKED_NAME PostscriptUserDataStack
#define DLINKED_LOWERCASE_NAME postscript_user_data_stack
#define DLINKED_TYPE PostscriptUserData
#include "arena/dlinked_impl.h"

struct PostscriptInterpreter {
    Arena* arena;

    PostscriptObjectList* operands;
    PostscriptObjectList* dict_stack; /// This stack is stored backwards, with
                                      /// index 0 being the top of the stack

    PostscriptResourceCategoryVec* resource_categories;
    PostscriptUserDataStack* user_data_stack;
};

Arena* postscript_interpreter_get_arena(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    return interpreter->arena;
}

PostscriptInterpreter* postscript_interpreter_new(Arena* arena) {
    RELEASE_ASSERT(arena);

    LOG_DIAG(INFO, PS, "Creating new postscript interpreter");

    PostscriptInterpreter* interpreter =
        arena_alloc(arena, sizeof(PostscriptInterpreter));
    interpreter->arena = arena;
    interpreter->operands = postscript_object_list_new(arena);
    interpreter->dict_stack = postscript_object_list_new(arena);
    interpreter->resource_categories =
        postscript_resource_category_vec_new(arena);
    interpreter->user_data_stack = postscript_user_data_stack_new(arena);

    // Initialize standard dictionaries
    postscript_interpreter_dict_push(
        interpreter,
        postscript_systemdict_ops(arena)
    );
    postscript_interpreter_dict_push(
        interpreter,
        (PostscriptObject
        ) {.type = POSTSCRIPT_OBJECT_DICT,
           .data.dict = postscript_object_list_new(arena),
           .access = POSTSCRIPT_ACCESS_UNLIMITED,
           .literal = true}
    );

    return interpreter;
}

PostscriptResourceCategoryVec* postscript_interpreter_get_resource_categories(
    PostscriptInterpreter* interpreter
) {
    RELEASE_ASSERT(interpreter);

    return interpreter->resource_categories;
}

void postscript_interpreter_add_operator(
    PostscriptInterpreter* interpreter,
    char* category_name,
    char* resource_name,
    PostscriptOperator
    operator,
    char * operator_name
) {
    RELEASE_ASSERT(interpreter);
    RELEASE_ASSERT(category_name);
    RELEASE_ASSERT(resource_name);
    RELEASE_ASSERT(operator);
    RELEASE_ASSERT(operator_name);

    // Get category
    PostscriptResourceCategory* category = postscript_get_resource_category(
        interpreter->resource_categories,
        category_name
    );
    if (!category) {
        category = postscript_resource_category_vec_push(
            interpreter->resource_categories,
            postscript_resource_category_new(interpreter->arena, category_name)
        );
    }

    // Get resource
    PostscriptResource* resource =
        postscript_resource_category_get_resource(category, resource_name);
    if (!resource) {
        resource = postscript_resource_vec_push(
            category->resources,
            postscript_resource_new_dict(interpreter->arena, resource_name)
        );
    }

    // Add operator
    postscript_resource_add_op(resource, operator, operator_name);
}

static PostscriptObjectList* get_current_object_list(
    const PostscriptInterpreter* interpreter,
    bool* is_sink
) {
    RELEASE_ASSERT(interpreter);

    PostscriptObject object;
    if (!postscript_object_list_back(interpreter->operands, &object)
        || object.type != POSTSCRIPT_OBJECT_SINK) {
        if (is_sink) {
            *is_sink = false;
        }
        return interpreter->operands;
    }

    if (is_sink) {
        *is_sink = true;
    }
    return object.data.sink.list;
}

PdfError* postscript_interpret_token(
    PostscriptInterpreter* interpreter,
    PostscriptToken token
) {
    RELEASE_ASSERT(interpreter);

    {
        Arena* str_arena = arena_new(128);
        LOG_DIAG(
            TRACE,
            PS,
            "Interpreting token: `%s`",
            postscript_string_as_cstr(token.text, str_arena)
        );
        arena_free(str_arena);
    }

    switch (token.type) {
        case POSTSCRIPT_TOKEN_INTEGER: {
            postscript_interpreter_operand_push(
                interpreter,
                (PostscriptObject
                ) {.type = POSTSCRIPT_OBJECT_INTEGER,
                   .data.integer = token.data.integer,
                   .access = POSTSCRIPT_ACCESS_UNLIMITED,
                   .literal = true}
            );
            break;
        }
        case POSTSCRIPT_TOKEN_REAL: {
            postscript_interpreter_operand_push(
                interpreter,
                (PostscriptObject
                ) {.type = POSTSCRIPT_OBJECT_REAL,
                   .data.real = token.data.real,
                   .access = POSTSCRIPT_ACCESS_UNLIMITED,
                   .literal = true}
            );
            break;
        }
        case POSTSCRIPT_TOKEN_LIT_STRING:
        case POSTSCRIPT_TOKEN_HEX_STRING: {
            postscript_interpreter_operand_push(
                interpreter,
                (PostscriptObject
                ) {.type = POSTSCRIPT_OBJECT_STRING,
                   .data.string = token.data.string,
                   .access = POSTSCRIPT_ACCESS_UNLIMITED,
                   .literal = true}
            );
            break;
        }
        case POSTSCRIPT_TOKEN_EXE_NAME: {
            PostscriptObject name_object = {
                .type = POSTSCRIPT_OBJECT_NAME,
                .data.name = token.data.name,
                .access = POSTSCRIPT_ACCESS_UNLIMITED,
                .literal = false
            };

            PDF_PROPAGATE(postscript_object_execute(interpreter, &name_object));
            break;
        }
        case POSTSCRIPT_TOKEN_LIT_NAME: {
            bool is_true = strcmp(token.data.name, "true") == 0;
            bool is_false = strcmp(token.data.name, "false") == 0;
            bool is_null = strcmp(token.data.name, "null") == 0;

            if (is_true || is_false) {
                postscript_interpreter_operand_push(
                    interpreter,
                    (PostscriptObject
                    ) {.type = POSTSCRIPT_OBJECT_BOOLEAN,
                       .data.boolean = is_true,
                       .access = POSTSCRIPT_ACCESS_UNLIMITED,
                       .literal = true}
                );
            } else if (is_null) {
                postscript_interpreter_operand_push(
                    interpreter,
                    (PostscriptObject
                    ) {.type = POSTSCRIPT_OBJECT_NAME,
                       .access = POSTSCRIPT_ACCESS_UNLIMITED,
                       .literal = true}
                );
            } else {
                postscript_interpreter_operand_push(
                    interpreter,
                    (PostscriptObject
                    ) {.type = POSTSCRIPT_OBJECT_NAME,
                       .data.name = token.data.name,
                       .access = POSTSCRIPT_ACCESS_UNLIMITED,
                       .literal = true}
                );
            }
            break;
        }
        case POSTSCRIPT_TOKEN_START_ARRAY: {
            PostscriptObjectList* array =
                postscript_object_list_new(interpreter->arena);
            postscript_interpreter_operand_push(
                interpreter,
                (PostscriptObject
                ) {.type = POSTSCRIPT_OBJECT_SINK,
                   .data.sink.list = array,
                   .data.sink.type = POSTSCRIPT_SINK_ARRAY,
                   .data.sink.sink_name = "array",
                   .access = POSTSCRIPT_ACCESS_UNLIMITED,
                   .literal = true}
            );
            break;
        }
        case POSTSCRIPT_TOKEN_END_ARRAY: {
            PostscriptObject sink;
            PDF_PROPAGATE(postscript_interpreter_pop_operand_typed(
                interpreter,
                POSTSCRIPT_OBJECT_SINK,
                true,
                &sink
            ));

            if (sink.data.sink.type != POSTSCRIPT_SINK_ARRAY) {
                return PDF_ERROR(
                    PDF_ERR_POSTSCRIPT_OPERAND_TYPE,
                    "Wrong sink type"
                );
            }

            postscript_interpreter_operand_push(
                interpreter,
                (PostscriptObject
                ) {.type = POSTSCRIPT_OBJECT_ARRAY,
                   .data.array = sink.data.sink.list,
                   .access = POSTSCRIPT_ACCESS_UNLIMITED,
                   .literal = true}
            );
            break;
        }
        default: {
            LOG_TODO("Unimplemented postscript token %d", (int)token.type);
        }
    }

    return NULL;
}

PdfError* postscript_interpreter_pop_operand(
    PostscriptInterpreter* interpreter,
    PostscriptObject* object_out
) {
    RELEASE_ASSERT(interpreter);
    RELEASE_ASSERT(object_out);

    if (!postscript_object_list_pop_back(interpreter->operands, object_out)) {
        return PDF_ERROR(PDF_ERR_POSTSCRIPT_OPERANDS_EMPTY);
    }

    LOG_DIAG(
        TRACE,
        PS,
        "Popped operand of type %s from operand stack (new_len=%zu)",
        postscript_object_name_lookup[object_out->type],
        postscript_object_list_len(interpreter->operands)
    );

    return NULL;
}

PdfError* postscript_interpreter_pop_operand_typed(
    PostscriptInterpreter* interpreter,
    PostscriptObjectType expected_type,
    bool literal,
    PostscriptObject* object_out
) {
    RELEASE_ASSERT(interpreter);
    RELEASE_ASSERT(object_out);

    PDF_PROPAGATE(postscript_interpreter_pop_operand(interpreter, object_out));
    if (object_out->type != expected_type) {
        return PDF_ERROR(
            PDF_ERR_POSTSCRIPT_OPERAND_TYPE,
            "Incorrect operand type: expected %d, found %d",
            (int)expected_type,
            (int)object_out->type
        );
    } else if (object_out->literal != literal) {
        return PDF_ERROR(
            PDF_ERR_POSTSCRIPT_OPERAND_TYPE,
            "Incorrect operand type: expected literal=%d, found literal=%d",
            (int)literal,
            (int)object_out->literal
        );
    }

    return NULL;
}

void postscript_interpreter_operand_push(
    PostscriptInterpreter* interpreter,
    PostscriptObject object
) {
    RELEASE_ASSERT(interpreter);

    bool is_sink;
    PostscriptObjectList* current_object_list =
        get_current_object_list(interpreter, &is_sink);
    postscript_object_list_push_back(current_object_list, object);

    if (is_sink) {
        LOG_DIAG(
            TRACE,
            PS,
            "Pushed operand of type %s to sink (new_len=%zu)",
            postscript_object_name_lookup[object.type],
            postscript_object_list_len(current_object_list)
        );
    } else {
        LOG_DIAG(
            TRACE,
            PS,
            "Pushed operand of type %s to operand stack (new_len=%zu)",
            postscript_object_name_lookup[object.type],
            postscript_object_list_len(current_object_list)
        );
    }
}

void postscript_interpreter_dict_push(
    PostscriptInterpreter* interpreter,
    PostscriptObject dictionary
) {
    RELEASE_ASSERT(interpreter);
    RELEASE_ASSERT(dictionary.type == POSTSCRIPT_OBJECT_DICT);

    LOG_DIAG(INFO, PS, "Pushing dictionary to dict stack");
    postscript_object_list_push_front(interpreter->dict_stack, dictionary);
}

PdfError* postscript_interpreter_dict_pop(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    if (postscript_object_list_len(interpreter->dict_stack) <= 2) {
        return PDF_ERROR(
            PDF_ERR_POSTSCRIPT_POP_STANDARD_DICT,
            "Cannot pop userdict from dictionary stack"
        );
    }

    PostscriptObject dict;
    RELEASE_ASSERT(
        postscript_object_list_pop_front(interpreter->dict_stack, &dict)
    );

    return NULL;
}

PdfError* postscript_interpreter_dict_entry(
    const PostscriptInterpreter* interpreter,
    const PostscriptObject* key,
    PostscriptObject* value_out
) {
    RELEASE_ASSERT(interpreter);
    RELEASE_ASSERT(key);
    RELEASE_ASSERT(value_out);

    if (key->type == POSTSCRIPT_OBJECT_STRING) {
        LOG_TODO("String to name conversion for postscript dict lookup");
    }

    for (size_t dict_idx = 0;
         dict_idx < postscript_object_list_len(interpreter->dict_stack);
         dict_idx++) {
        PostscriptObject dict;
        RELEASE_ASSERT(
            postscript_object_list_get(interpreter->dict_stack, dict_idx, &dict)
        );
        RELEASE_ASSERT(dict.type == POSTSCRIPT_OBJECT_DICT);

        for (size_t idx = 0; idx < postscript_object_list_len(dict.data.dict);
             idx += 2) {
            PostscriptObject entry_key;
            RELEASE_ASSERT(
                postscript_object_list_get(dict.data.dict, idx, &entry_key)
            );

            if (entry_key.type == POSTSCRIPT_OBJECT_STRING) {
                LOG_TODO("String to name conversion for postscript dicts");
            }

            if (postscript_object_eq(key, &entry_key)) {
                RELEASE_ASSERT(postscript_object_list_get(
                    dict.data.dict,
                    idx + 1,
                    value_out
                ));

                return NULL;
            }
        }
    }

    return PDF_ERROR(
        PDF_ERR_POSTSCRIPT_KEY_MISSING,
        "Entry with key `%s` not found in dictionary",
        postscript_object_fmt(interpreter->arena, key)
    );
}

PdfError* postscript_interpreter_define(
    PostscriptInterpreter* interpreter,
    PostscriptObject key,
    PostscriptObject value
) {
    RELEASE_ASSERT(interpreter);

    PostscriptObject current_dict;
    RELEASE_ASSERT(
        postscript_object_list_get(interpreter->dict_stack, 0, &current_dict)
    );
    RELEASE_ASSERT(current_dict.type == POSTSCRIPT_OBJECT_DICT);

    if (current_dict.access != POSTSCRIPT_ACCESS_UNLIMITED) {
        return PDF_ERROR(
            PDF_ERR_POSTSCRIPT_ACCESS_VIOLATION,
            "The current top of the dictionary stack doesn't have write access"
        );
    }

    postscript_object_list_push_back(current_dict.data.dict, key);
    postscript_object_list_push_back(current_dict.data.dict, value);

    return NULL;
}

PdfError* postscript_interpreter_user_data(
    PostscriptInterpreter* interpreter,
    const char* expected_name,
    void** user_data_out
) {
    RELEASE_ASSERT(interpreter);
    RELEASE_ASSERT(expected_name);
    RELEASE_ASSERT(user_data_out);

    PostscriptUserData user_data_top;
    if (!postscript_user_data_stack_back(
            interpreter->user_data_stack,
            &user_data_top
        )) {
        return PDF_ERROR(
            PDF_ERR_POSTSCRIPT_USER_DATA_INVALID,
            "The user-data stack is empty"
        );
    }

    if (strcmp(expected_name, user_data_top.name) != 0) {
        return PDF_ERROR(
            PDF_ERR_POSTSCRIPT_USER_DATA_INVALID,
            "The user-data at the top of the stack had an unexpected name"
        );
    }

    *user_data_out = user_data_top.data;

    return NULL;
}

/// Pushes a new user-data to the stack.
void postscript_interpreter_user_data_push(
    PostscriptInterpreter* interpreter,
    void* user_data,
    const char* name
) {
    RELEASE_ASSERT(interpreter);
    RELEASE_ASSERT(user_data);
    RELEASE_ASSERT(name);

    postscript_user_data_stack_push_back(
        interpreter->user_data_stack,
        (PostscriptUserData) {.data = user_data, .name = name}
    );
}

/// Pops a user-data from the stack.
PdfError* postscript_interpreter_user_data_pop(
    PostscriptInterpreter* interpreter,
    const char* expected_name
) {
    RELEASE_ASSERT(interpreter);
    RELEASE_ASSERT(expected_name);

    PostscriptUserData user_data_top;
    if (!postscript_user_data_stack_pop_back(
            interpreter->user_data_stack,
            &user_data_top
        )) {
        return PDF_ERROR(
            PDF_ERR_POSTSCRIPT_USER_DATA_INVALID,
            "The user-data stack is empty"
        );
    }

    if (strcmp(expected_name, user_data_top.name) != 0) {
        return PDF_ERROR(
            PDF_ERR_POSTSCRIPT_USER_DATA_INVALID,
            "The user-data at the top of the stack had an unexpected name"
        );
    }

    return NULL;
}
