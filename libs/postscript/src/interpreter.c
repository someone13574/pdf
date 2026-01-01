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

#define DLINKED_NAME PSListStack
#define DLINKED_LOWERCASE_NAME ps_list_stack
#define DLINKED_TYPE PSObjectList*
#include "arena/dlinked_impl.h"

typedef struct {
    const char* name;
    void* data;
} PSUserData;

#define DLINKED_NAME PSUserDataStack
#define DLINKED_LOWERCASE_NAME ps_user_data_stack
#define DLINKED_TYPE PSUserData
#include "arena/dlinked_impl.h"

struct PSInterpreter {
    Arena* arena;

    PSObjectList* operands;
    PSObjectList* dict_stack;

    PSResourceCategoryVec* resource_categories;
    PSUserDataStack* user_data_stack;
};

Arena* ps_interpreter_get_arena(PSInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    return interpreter->arena;
}

static void init_default_resource_categories(PSInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    // TODO: non-dict categories
    char* category_names[] = {
        "Font",
        "CIDFont",
        "CMap",
        "FontSet",
        "Form",
        "Pattern",
        "ProcSet",
        "Halftone",
        "ColorRendering",
        "IdiomSet",
        "InkParams",
        "TrapParams",
        "OutputDevice",
        "ControlLanguage",
        "Localization",
        "PDL",
        "HWOptions",
        "Category"
    };

    for (size_t idx = 0; idx < sizeof(category_names) / sizeof(char*); idx++) {
        if (!ps_get_resource_category(
                interpreter->resource_categories,
                category_names[idx]
            )) {
            ps_resource_category_vec_push(
                interpreter->resource_categories,
                ps_resource_category_new(
                    interpreter->arena,
                    category_names[idx]
                )
            );
        }
    }
}

PSInterpreter* ps_interpreter_new(Arena* arena) {
    RELEASE_ASSERT(arena);

    LOG_DIAG(INFO, PS, "Creating new postscript interpreter");

    PSInterpreter* interpreter = arena_alloc(arena, sizeof(PSInterpreter));
    interpreter->arena = arena;
    interpreter->operands = ps_object_list_new(arena);
    interpreter->dict_stack = ps_object_list_new(arena);
    interpreter->resource_categories = ps_resource_category_vec_new(arena);
    init_default_resource_categories(interpreter);
    interpreter->user_data_stack = ps_user_data_stack_new(arena);

    // Initialize standard dictionaries
    ps_interpreter_dict_push(interpreter, ps_systemdict_ops(arena));
    ps_interpreter_dict_push(
        interpreter,
        (PSObject) {.type = PS_OBJECT_DICT,
                    .data.dict = ps_object_list_new(arena),
                    .access = PS_ACCESS_UNLIMITED,
                    .literal = true}
    );

    return interpreter;
}

void ps_interpreter_dump(PSInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    Arena* fmt_arena = arena_new(1024);
    LOG_DIAG(INFO, PS, "Interpreter state:");

    // Resources
    LOG_DIAG(INFO, PS, " ");
    LOG_DIAG(INFO, PS, " ");
    LOG_DIAG(INFO, PS, "    Resources:");
    for (size_t category_idx = 0;
         category_idx
         < ps_resource_category_vec_len(interpreter->resource_categories);
         category_idx++) {
        PSResourceCategory category;
        RELEASE_ASSERT(ps_resource_category_vec_get(
            interpreter->resource_categories,
            category_idx,
            &category
        ));

        for (size_t resource_idx = 0;
             resource_idx < ps_resource_vec_len(category.resources);
             resource_idx++) {
            PSResource resource;
            RELEASE_ASSERT(
                ps_resource_vec_get(category.resources, resource_idx, &resource)
            );

            LOG_DIAG(
                INFO,
                PS,
                "        %s/%s: %s",
                category.name,
                resource.name,
                ps_object_fmt(fmt_arena, &resource.object)
            );
        }
    }

    // Dictionaries
    LOG_DIAG(INFO, PS, " ");
    LOG_DIAG(INFO, PS, " ");
    LOG_DIAG(
        INFO,
        PS,
        "    Dictionary stack (len=%zu):",
        ps_object_list_len(interpreter->dict_stack)
    );
    for (size_t idx = 0; idx < ps_object_list_len(interpreter->dict_stack);
         idx++) {
        PSObject* stack_object = NULL;
        RELEASE_ASSERT(
            ps_object_list_get_ptr(interpreter->dict_stack, idx, &stack_object)
        );

        LOG_DIAG(
            INFO,
            PS,
            "       %zu: %s",
            idx,
            ps_object_fmt(fmt_arena, stack_object)
        );
    }

    // Operands
    LOG_DIAG(INFO, PS, " ");
    LOG_DIAG(INFO, PS, " ");
    LOG_DIAG(
        INFO,
        PS,
        "   Operand stack (len=%zu):",
        ps_object_list_len(interpreter->operands)
    );
    for (size_t idx = 0; idx < ps_object_list_len(interpreter->operands);
         idx++) {
        PSObject* stack_object = NULL;
        RELEASE_ASSERT(
            ps_object_list_get_ptr(interpreter->operands, idx, &stack_object)
        );

        LOG_DIAG(
            INFO,
            PS,
            "       %zu: %s",
            idx,
            ps_object_fmt(fmt_arena, stack_object)
        );
    }

    arena_free(fmt_arena);
}

PSResourceCategoryVec*
ps_interpreter_get_resource_categories(PSInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    return interpreter->resource_categories;
}

void ps_interpreter_add_operator(
    PSInterpreter* interpreter,
    char* category_name,
    char* resource_name,
    PSOperator operator,
    char* operator_name
) {
    RELEASE_ASSERT(interpreter);
    RELEASE_ASSERT(category_name);
    RELEASE_ASSERT(resource_name);
    RELEASE_ASSERT(operator);
    RELEASE_ASSERT(operator_name);

    // Get category
    PSResourceCategory* category = ps_get_resource_category(
        interpreter->resource_categories,
        category_name
    );
    if (!category) {
        category = ps_resource_category_vec_push(
            interpreter->resource_categories,
            ps_resource_category_new(interpreter->arena, category_name)
        );
    }

    // Get resource
    PSResource* resource =
        ps_resource_category_get_resource(category, resource_name);
    if (!resource) {
        resource = ps_resource_vec_push(
            category->resources,
            ps_resource_new_dict(interpreter->arena, resource_name)
        );
    }

    // Add operator
    ps_resource_add_op(resource, operator, operator_name);
}

static PSObjectList* get_current_object_list(
    const PSInterpreter* interpreter,
    bool* is_sink,
    bool* is_dict
) {
    RELEASE_ASSERT(interpreter);

    PSObject object;
    if (!ps_object_list_back(interpreter->operands, &object)
        || object.type != PS_OBJECT_SINK) {
        if (is_sink) {
            *is_sink = false;
        }
        if (is_dict) {
            *is_dict = false;
        }
        return interpreter->operands;
    }

    if (is_sink) {
        *is_sink = true;
    }
    if (is_dict) {
        *is_dict = object.data.sink.type == PS_SINK_DICT;
    }
    return object.data.sink.list;
}

PdfError* ps_interpret_token(PSInterpreter* interpreter, PSToken token) {
    RELEASE_ASSERT(interpreter);

    switch (token.type) {
        case PS_TOKEN_INTEGER: {
            ps_interpreter_operand_push(
                interpreter,
                (PSObject) {.type = PS_OBJECT_INTEGER,
                            .data.integer = token.data.integer,
                            .access = PS_ACCESS_UNLIMITED,
                            .literal = true}
            );
            break;
        }
        case PS_TOKEN_REAL: {
            ps_interpreter_operand_push(
                interpreter,
                (PSObject) {.type = PS_OBJECT_REAL,
                            .data.real = token.data.real,
                            .access = PS_ACCESS_UNLIMITED,
                            .literal = true}
            );
            break;
        }
        case PS_TOKEN_LIT_STRING:
        case PS_TOKEN_HEX_STRING: {
            ps_interpreter_operand_push(
                interpreter,
                (PSObject) {.type = PS_OBJECT_STRING,
                            .data.string = token.data.string,
                            .access = PS_ACCESS_UNLIMITED,
                            .literal = true}
            );
            break;
        }
        case PS_TOKEN_EXE_NAME: {
            PSObject name_object = {
                .type = PS_OBJECT_NAME,
                .data.name = token.data.name,
                .access = PS_ACCESS_UNLIMITED,
                .literal = false
            };

            PDF_PROPAGATE(ps_interpret_object(interpreter, name_object));
            break;
        }
        case PS_TOKEN_LIT_NAME: {
            bool is_true = strcmp(token.data.name, "true") == 0;
            bool is_false = strcmp(token.data.name, "false") == 0;
            bool is_null = strcmp(token.data.name, "null") == 0;

            if (is_true || is_false) {
                ps_interpreter_operand_push(
                    interpreter,
                    (PSObject) {.type = PS_OBJECT_BOOLEAN,
                                .data.boolean = is_true,
                                .access = PS_ACCESS_UNLIMITED,
                                .literal = true}
                );
            } else if (is_null) {
                ps_interpreter_operand_push(
                    interpreter,
                    (PSObject) {.type = PS_OBJECT_NAME,
                                .access = PS_ACCESS_UNLIMITED,
                                .literal = true}
                );
            } else {
                ps_interpreter_operand_push(
                    interpreter,
                    (PSObject) {.type = PS_OBJECT_NAME,
                                .data.name = token.data.name,
                                .access = PS_ACCESS_UNLIMITED,
                                .literal = true}
                );
            }
            break;
        }
        case PS_TOKEN_START_ARRAY: {
            PSObjectList* array = ps_object_list_new(interpreter->arena);
            ps_interpreter_operand_push(
                interpreter,
                (PSObject) {.type = PS_OBJECT_SINK,
                            .data.sink.list = array,
                            .data.sink.type = PS_SINK_ARRAY,
                            .data.sink.sink_name = "array",
                            .access = PS_ACCESS_UNLIMITED,
                            .literal = true}
            );
            break;
        }
        case PS_TOKEN_END_ARRAY: {
            PSObject sink;
            PDF_PROPAGATE(ps_interpreter_pop_operand_typed(
                interpreter,
                PS_OBJECT_SINK,
                true,
                &sink
            ));

            if (sink.data.sink.type != PS_SINK_ARRAY) {
                return PDF_ERROR(PS_ERR_OPERAND_TYPE, "Wrong sink type");
            }

            ps_interpreter_operand_push(
                interpreter,
                (PSObject) {.type = PS_OBJECT_ARRAY,
                            .data.array = sink.data.sink.list,
                            .access = PS_ACCESS_UNLIMITED,
                            .literal = true}
            );
            break;
        }
        case PS_TOKEN_START_PROC: {
            PSObjectList* proc = ps_object_list_new(interpreter->arena);
            ps_interpreter_operand_push(
                interpreter,
                (PSObject) {.type = PS_OBJECT_SINK,
                            .data.sink.list = proc,
                            .data.sink.type = PS_SINK_PROC,
                            .data.sink.sink_name = "proc",
                            .access = PS_ACCESS_UNLIMITED,
                            .literal = true}
            );
            break;
        }
        case PS_TOKEN_END_PROC: {
            PSObject sink;
            PDF_PROPAGATE(ps_interpreter_pop_operand_typed(
                interpreter,
                PS_OBJECT_SINK,
                true,
                &sink
            ));

            if (sink.data.sink.type != PS_SINK_PROC) {
                return PDF_ERROR(PS_ERR_OPERAND_TYPE, "Wrong sink type");
            }

            ps_interpreter_operand_push(
                interpreter,
                (PSObject) {.type = PS_OBJECT_PROC,
                            .data.proc = sink.data.sink.list,
                            .access = PS_ACCESS_UNLIMITED,
                            .literal = true}
            );
            break;
        }
        case PS_TOKEN_START_DICT: {
            PSObjectList* dict = ps_object_list_new(interpreter->arena);
            ps_interpreter_operand_push(
                interpreter,
                (PSObject) {.type = PS_OBJECT_SINK,
                            .data.sink.list = dict,
                            .data.sink.type = PS_SINK_DICT,
                            .data.sink.sink_name = "dict",
                            .access = PS_ACCESS_UNLIMITED,
                            .literal = true}
            );
            break;
        }
        case PS_TOKEN_END_DICT: {
            PSObject sink;
            PDF_PROPAGATE(ps_interpreter_pop_operand_typed(
                interpreter,
                PS_OBJECT_SINK,
                true,
                &sink
            ));

            if (sink.data.sink.type != PS_SINK_DICT) {
                return PDF_ERROR(PS_ERR_OPERAND_TYPE, "Wrong sink type");
            }

            if (ps_object_list_len(sink.data.sink.list) % 2 != 0) {
                return PDF_ERROR(
                    PS_ERR_OPERAND_TYPE,
                    "Invalid number of object in dict"
                );
            }

            ps_interpreter_operand_push(
                interpreter,
                (PSObject) {.type = PS_OBJECT_DICT,
                            .data.dict = sink.data.sink.list,
                            .access = PS_ACCESS_UNLIMITED,
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

PdfError*
ps_interpret_tokens(PSInterpreter* interpreter, PSTokenizer* tokenizer) {
    RELEASE_ASSERT(interpreter);
    RELEASE_ASSERT(tokenizer);

    while (true) {
        bool got_token;
        PSToken token;
        PDF_PROPAGATE(ps_next_token(tokenizer, &token, &got_token));

        if (!got_token) {
            break;
        }

        PDF_PROPAGATE(ps_interpret_token(interpreter, token));
    }

    return NULL;
}

PdfError* ps_interpret_object(PSInterpreter* interpreter, PSObject object) {
    RELEASE_ASSERT(interpreter);

    if (object.literal) {
        ps_interpreter_operand_push(interpreter, object);
    } else {
        PSObject proc_object;
        if (ps_object_list_back(interpreter->operands, &proc_object)
            && proc_object.type == PS_OBJECT_SINK
            && proc_object.data.sink.type == PS_SINK_PROC) {
            ps_object_list_push_back(proc_object.data.sink.list, object);
        } else {
            PDF_PROPAGATE(ps_object_execute(interpreter, &object));
        }
    }

    return NULL;
}

PSObjectList* ps_interpreter_stack(PSInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);
    return interpreter->operands;
}

PdfError*
ps_interpreter_pop_operand(PSInterpreter* interpreter, PSObject* object_out) {
    RELEASE_ASSERT(interpreter);
    RELEASE_ASSERT(object_out);

    if (!ps_object_list_pop_back(interpreter->operands, object_out)) {
        return PDF_ERROR(PS_ERR_OPERANDS_EMPTY, "No operands to pop");
    }

    LOG_DIAG(
        TRACE,
        PS,
        "Popped operand of type %s from operand stack (new_len=%zu)",
        ps_object_name_lookup[object_out->type],
        ps_object_list_len(interpreter->operands)
    );

    return NULL;
}

PdfError* ps_interpreter_pop_operand_typed(
    PSInterpreter* interpreter,
    PSObjectType expected_type,
    bool literal,
    PSObject* object_out
) {
    RELEASE_ASSERT(interpreter);
    RELEASE_ASSERT(object_out);

    PDF_PROPAGATE(ps_interpreter_pop_operand(interpreter, object_out));
    if (object_out->type != expected_type) {
        return PDF_ERROR(
            PS_ERR_OPERAND_TYPE,
            "Incorrect operand type: expected %d, found %d",
            (int)expected_type,
            (int)object_out->type
        );
    } else if (object_out->literal != literal) {
        return PDF_ERROR(
            PS_ERR_OPERAND_TYPE,
            "Incorrect operand type: expected literal=%d, found literal=%d",
            (int)literal,
            (int)object_out->literal
        );
    }

    return NULL;
}

void ps_interpreter_operand_push(PSInterpreter* interpreter, PSObject object) {
    RELEASE_ASSERT(interpreter);

    bool is_sink;
    bool is_dict;
    PSObjectList* current_object_list =
        get_current_object_list(interpreter, &is_sink, &is_dict);
    if (is_dict && object.type == PS_OBJECT_STRING) {
        object.type = PS_OBJECT_NAME;
        object.data.name =
            ps_string_as_cstr(object.data.string, interpreter->arena);
    }

    ps_object_list_push_back(current_object_list, object);

    if (is_sink) {
        LOG_DIAG(
            TRACE,
            PS,
            "Pushed operand of type %s to sink (new_len=%zu)",
            ps_object_name_lookup[object.type],
            ps_object_list_len(current_object_list)
        );
    } else {
        LOG_DIAG(
            TRACE,
            PS,
            "Pushed operand of type %s to operand stack (new_len=%zu)",
            ps_object_name_lookup[object.type],
            ps_object_list_len(current_object_list)
        );
    }
}

void ps_interpreter_dict(PSInterpreter* interpreter, PSObject* object_out) {
    RELEASE_ASSERT(interpreter);
    RELEASE_ASSERT(object_out);

    RELEASE_ASSERT(ps_object_list_back(interpreter->dict_stack, object_out));
}

void ps_interpreter_dict_push(PSInterpreter* interpreter, PSObject dictionary) {
    RELEASE_ASSERT(interpreter);
    RELEASE_ASSERT(dictionary.type == PS_OBJECT_DICT);

    LOG_DIAG(INFO, PS, "Pushing dictionary to dict stack");
    ps_object_list_push_back(interpreter->dict_stack, dictionary);
}

PdfError* ps_interpreter_dict_pop(PSInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    if (ps_object_list_len(interpreter->dict_stack) <= 2) {
        return PDF_ERROR(
            PS_ERR_POP_STANDARD_DICT,
            "Cannot pop userdict from dictionary stack"
        );
    }

    PSObject dict;
    RELEASE_ASSERT(ps_object_list_pop_back(interpreter->dict_stack, &dict));

    return NULL;
}

PdfError* ps_interpreter_dict_entry(
    const PSInterpreter* interpreter,
    const PSObject* key,
    PSObject* value_out
) {
    RELEASE_ASSERT(interpreter);
    RELEASE_ASSERT(key);
    RELEASE_ASSERT(value_out);

    if (key->type == PS_OBJECT_STRING) {
        LOG_TODO("String to name conversion for postscript dict lookup");
    }

    for (size_t dict_idx = ps_object_list_len(interpreter->dict_stack);
         dict_idx-- > 0;) {
        PSObject dict;
        RELEASE_ASSERT(
            ps_object_list_get(interpreter->dict_stack, dict_idx, &dict)
        );
        RELEASE_ASSERT(dict.type == PS_OBJECT_DICT);

        for (size_t idx = 0; idx < ps_object_list_len(dict.data.dict);
             idx += 2) {
            PSObject entry_key;
            RELEASE_ASSERT(ps_object_list_get(dict.data.dict, idx, &entry_key));

            if (entry_key.type == PS_OBJECT_STRING) {
                LOG_TODO("String to name conversion for postscript dicts");
            }

            if (ps_object_eq(key, &entry_key)) {
                RELEASE_ASSERT(
                    ps_object_list_get(dict.data.dict, idx + 1, value_out)
                );

                return NULL;
            }
        }
    }

    return PDF_ERROR(
        PS_ERR_KEY_MISSING,
        "Entry with key `%s` not found in dictionary stack",
        ps_object_fmt(interpreter->arena, key)
    );
}

PdfError* ps_interpreter_define(
    PSInterpreter* interpreter,
    PSObject key,
    PSObject value
) {
    RELEASE_ASSERT(interpreter);

    PSObject current_dict;
    RELEASE_ASSERT(ps_object_list_back(interpreter->dict_stack, &current_dict));
    RELEASE_ASSERT(current_dict.type == PS_OBJECT_DICT);

    if (current_dict.access != PS_ACCESS_UNLIMITED) {
        return PDF_ERROR(
            PS_ERR_ACCESS_VIOLATION,
            "The current top of the dictionary stack doesn't have write access"
        );
    }

    if (value.type == PS_OBJECT_PROC) {
        value.literal = false;
    }

    ps_object_list_push_back(current_dict.data.dict, key);
    ps_object_list_push_back(current_dict.data.dict, value);

    return NULL;
}

PdfError* ps_interpreter_user_data(
    PSInterpreter* interpreter,
    const char* expected_name,
    void** user_data_out
) {
    RELEASE_ASSERT(interpreter);
    RELEASE_ASSERT(expected_name);
    RELEASE_ASSERT(user_data_out);

    PSUserData user_data_top;
    if (!ps_user_data_stack_back(
            interpreter->user_data_stack,
            &user_data_top
        )) {
        return PDF_ERROR(
            PS_ERR_USER_DATA_INVALID,
            "The user-data stack is empty"
        );
    }

    if (strcmp(expected_name, user_data_top.name) != 0) {
        return PDF_ERROR(
            PS_ERR_USER_DATA_INVALID,
            "The user-data at the top of the stack had an unexpected name"
        );
    }

    *user_data_out = user_data_top.data;

    return NULL;
}

/// Pushes a new user-data to the stack.
void ps_interpreter_user_data_push(
    PSInterpreter* interpreter,
    void* user_data,
    const char* name
) {
    RELEASE_ASSERT(interpreter);
    RELEASE_ASSERT(user_data);
    RELEASE_ASSERT(name);

    ps_user_data_stack_push_back(
        interpreter->user_data_stack,
        (PSUserData) {.data = user_data, .name = name}
    );
}

/// Pops a user-data from the stack.
PdfError* ps_interpreter_user_data_pop(
    PSInterpreter* interpreter,
    const char* expected_name
) {
    RELEASE_ASSERT(interpreter);
    RELEASE_ASSERT(expected_name);

    PSUserData user_data_top;
    if (!ps_user_data_stack_pop_back(
            interpreter->user_data_stack,
            &user_data_top
        )) {
        return PDF_ERROR(
            PS_ERR_USER_DATA_INVALID,
            "The user-data stack is empty"
        );
    }

    if (strcmp(expected_name, user_data_top.name) != 0) {
        return PDF_ERROR(
            PS_ERR_USER_DATA_INVALID,
            "The user-data at the top of the stack had an unexpected name"
        );
    }

    return NULL;
}
