#include "postscript/interpreter.h"

#include <string.h>

#include "arena/arena.h"
#include "logger/log.h"
#include "pdf_error/error.h"
#include "postscript/object.h"
#include "postscript/resource.h"
#include "postscript/tokenizer.h"
#include "procedures.h"

#define DLINKED_NAME PostscriptListStack
#define DLINKED_LOWERCASE_NAME postscript_list_stack
#define DLINKED_TYPE PostscriptObjectList*
#include "arena/dlinked_impl.h"

struct PostscriptInterpreter {
    Arena* arena;

    PostscriptObjectList* operands;
    PostscriptObjectList* dict_stack; /// This stack is stored backwards, with
                                      /// index 0 being the top of the stack
    PostscriptResourceCategoryVec* resource_categories;
    PostscriptListStack* list_construction_stack;
};

PostscriptInterpreter* postscript_interpreter_new(Arena* arena) {
    RELEASE_ASSERT(arena);

    PostscriptInterpreter* interpreter =
        arena_alloc(arena, sizeof(PostscriptInterpreter));
    interpreter->arena = arena;
    interpreter->operands = postscript_object_list_new(arena);
    interpreter->dict_stack = postscript_object_list_new(arena);
    interpreter->resource_categories =
        postscript_resource_category_vec_new(arena);
    interpreter->list_construction_stack = postscript_list_stack_new(arena);

    // Initialize systemdict
    postscript_object_list_push_front(
        interpreter->dict_stack,
        postscript_systemdict_procedures(arena)
    );

    return interpreter;
}

PostscriptResourceCategoryVec* postscript_interpreter_get_resource_categories(
    PostscriptInterpreter* interpreter
) {
    RELEASE_ASSERT(interpreter);

    return interpreter->resource_categories;
}

void postscript_interpreter_add_proc(
    PostscriptInterpreter* interpreter,
    char* category_name,
    char* resource_name,
    PostscriptCustomProcedure procedure,
    char* procedure_name
) {
    RELEASE_ASSERT(interpreter);
    RELEASE_ASSERT(category_name);
    RELEASE_ASSERT(resource_name);
    RELEASE_ASSERT(procedure);

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

    // Add procedure
    postscript_resource_add_fn(resource, procedure_name, procedure);
}

static PostscriptObjectList*
get_current_object_list(const PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    size_t stack_len =
        postscript_list_stack_len(interpreter->list_construction_stack);
    if (stack_len == 0) {
        return interpreter->operands;
    }

    PostscriptObjectList* current_construction_list = NULL;
    RELEASE_ASSERT(postscript_list_stack_get(
        interpreter->list_construction_stack,
        stack_len - 1,
        &current_construction_list
    ));
    return current_construction_list;
}

PdfError* postscript_interpret_token(
    PostscriptInterpreter* interpreter,
    PostscriptToken token
) {
    RELEASE_ASSERT(interpreter);

    PostscriptObjectList* current_object_list =
        get_current_object_list(interpreter);

    switch (token.type) {
        case POSTSCRIPT_TOKEN_INTEGER: {
            postscript_object_list_push_back(
                current_object_list,
                (PostscriptObject
                ) {.type = POSTSCRIPT_OBJECT_INTEGER,
                   .data.integer = token.data.integer,
                   .access = POSTSCRIPT_ACCESS_UNLIMITED,
                   .literal = true}
            );
            break;
        }
        case POSTSCRIPT_TOKEN_REAL: {
            postscript_object_list_push_back(
                current_object_list,
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
            postscript_object_list_push_back(
                current_object_list,
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

            if (is_true || is_false) {
                postscript_object_list_push_back(
                    current_object_list,
                    (PostscriptObject
                    ) {.type = POSTSCRIPT_OBJECT_BOOLEAN,
                       .data.boolean = is_true,
                       .access = POSTSCRIPT_ACCESS_UNLIMITED,
                       .literal = true}
                );
            } else {
                postscript_object_list_push_back(
                    current_object_list,
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
            postscript_list_stack_push_back(
                interpreter->list_construction_stack,
                array
            );
            break;
        }
        case POSTSCRIPT_TOKEN_END_ARRAY: {
            if (postscript_list_stack_len(interpreter->list_construction_stack)
                <= 1) {
                return PDF_ERROR(PDF_ERR_POSTSCRIPT_ARRAY_NOT_STARTED);
            }

            PostscriptObjectList* array;
            RELEASE_ASSERT(postscript_list_stack_pop_back(
                interpreter->list_construction_stack,
                &array
            ));

            current_object_list = get_current_object_list(interpreter);
            postscript_object_list_push_back(
                current_object_list,
                (PostscriptObject
                ) {.type = POSTSCRIPT_OBJECT_ARRAY,
                   .data.array = array,
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

    postscript_object_list_push_back(interpreter->operands, object);
}

void postscript_interpreter_dict_push(
    PostscriptInterpreter* interpreter,
    PostscriptObject dictionary
) {
    RELEASE_ASSERT(interpreter);
    RELEASE_ASSERT(dictionary.type == POSTSCRIPT_OBJECT_DICT);

    postscript_object_list_push_front(interpreter->dict_stack, dictionary);
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
