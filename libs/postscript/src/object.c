#include "postscript/object.h"

#include <string.h>

#include "arena/string.h"
#include "logger/log.h"
#include "pdf_error/error.h"
#include "postscript/interpreter.h"
#include "postscript/tokenizer.h"

#define DLINKED_NAME PostscriptObjectList
#define DLINKED_LOWERCASE_NAME postscript_object_list
#define DLINKED_TYPE PostscriptObject
#include "arena/dlinked_impl.h"

#define X(name) #name,
const char* postscript_object_name_lookup[] = {POSTSCRIPT_OBJECT_TYPES};
#undef X

PdfError* postscript_object_execute(
    PostscriptInterpreter* interpreter,
    const PostscriptObject* object
) {
    RELEASE_ASSERT(interpreter);
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(!object->literal);

    switch (object->type) {
        case POSTSCRIPT_OBJECT_NAME: {
            LOG_DIAG(DEBUG, PS, "Executing name `%s`", object->data.name);

            PostscriptObject dict_object;
            PDF_PROPAGATE(postscript_interpreter_dict_entry(
                interpreter,
                object,
                &dict_object
            ));

            if (dict_object.literal) {
                postscript_interpreter_operand_push(interpreter, dict_object);
                break;
            }

            PDF_PROPAGATE(postscript_object_execute(interpreter, &dict_object));
            break;
        }
        case POSTSCRIPT_OBJECT_OPERATOR: {
            LOG_DIAG(TRACE, PS, "Executing operator");

            PDF_PROPAGATE(
                object->data.operator(interpreter),
                "Error occurred while executing operator"
            );
            break;
        }
        default: {
            LOG_TODO("Execution of postscript type %d", (int)object->type);
        }
    }

    return NULL;
}

bool postscript_object_eq(
    const PostscriptObject* a,
    const PostscriptObject* b
) {
    RELEASE_ASSERT(a);
    RELEASE_ASSERT(b);

    if (a->type != b->type) {
        return false;
    }

    switch (a->type) {
        case POSTSCRIPT_OBJECT_NAME: {
            return strcmp(a->data.name, b->data.name) == 0;
        }
        default: {
            LOG_TODO("Postscript equality for type %d", (int)a->type);
        }
    }

    return false;
}

const char*
postscript_object_fmt(Arena* arena, const PostscriptObject* object) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(object);

    switch (object->type) {
        case POSTSCRIPT_OBJECT_INTEGER: {
            return arena_string_buffer(
                arena_string_new_fmt(arena, "%d", (int)object->data.integer)
            );
        }
        case POSTSCRIPT_OBJECT_NAME: {
            char* prefix = object->literal ? "/" : "";
            return arena_string_buffer(
                arena_string_new_fmt(arena, "%s%s", prefix, object->data.name)
            );
        }
        case POSTSCRIPT_OBJECT_REAL: {
            return arena_string_buffer(
                arena_string_new_fmt(arena, "%f", object->data.real)
            );
        }
        case POSTSCRIPT_OBJECT_ARRAY: {
            char* delims = object->literal ? "[]" : "{}";

            ArenaString* str = arena_string_new_fmt(arena, "%c ", delims[0]);
            for (size_t idx = 0;
                 idx < postscript_object_list_len(object->data.array);
                 idx++) {
                PostscriptObject* array_object = NULL;
                RELEASE_ASSERT(postscript_object_list_get_ptr(
                    object->data.array,
                    idx,
                    &array_object
                ));

                str = arena_string_new_fmt(
                    arena,
                    "%s%s ",
                    arena_string_buffer(str),
                    postscript_object_fmt(arena, array_object)
                );
            }

            return arena_string_buffer(arena_string_new_fmt(
                arena,
                "%s%c",
                arena_string_buffer(str),
                delims[1]
            ));
        }
        case POSTSCRIPT_OBJECT_OPERATOR: {
            return "<|builtin|>";
        }
        case POSTSCRIPT_OBJECT_DICT: {
            ArenaString* str = arena_string_new_fmt(arena, "<< ");
            for (size_t idx = 0;
                 idx < postscript_object_list_len(object->data.dict);
                 idx++) {
                PostscriptObject* dict_object = NULL;
                RELEASE_ASSERT(postscript_object_list_get_ptr(
                    object->data.dict,
                    idx,
                    &dict_object
                ));

                str = arena_string_new_fmt(
                    arena,
                    "%s%s ",
                    arena_string_buffer(str),
                    postscript_object_fmt(arena, dict_object)
                );
            }

            return arena_string_buffer(
                arena_string_new_fmt(arena, "%s>>", arena_string_buffer(str))
            );
        }
        case POSTSCRIPT_OBJECT_STRING: {
            return arena_string_buffer(arena_string_new_fmt(
                arena,
                "(%s)",
                postscript_string_as_cstr(object->data.string, arena)
            ));
        }
        default: {
            LOG_TODO(
                "Print postscript object type %s",
                postscript_object_name_lookup[object->type]
            );
        }
    }
}
