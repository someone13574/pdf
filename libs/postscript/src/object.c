#include "postscript/object.h"

#include <string.h>

#include "err/error.h"
#include "logger/log.h"
#include "postscript/interpreter.h"
#include "postscript/tokenizer.h"
#include "str/alloc_str.h"

#define DLINKED_NAME PSObjectList
#define DLINKED_LOWERCASE_NAME ps_object_list
#define DLINKED_TYPE PSObject
#include "arena/dlinked_impl.h"

#define X(name) #name,
const char* ps_object_name_lookup[] = {PS_OBJECT_TYPES};
#undef X

Error* ps_object_execute(PSInterpreter* interpreter, const PSObject* object) {
    RELEASE_ASSERT(interpreter);
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(!object->literal);

    switch (object->type) {
        case PS_OBJECT_NAME: {
            LOG_DIAG(DEBUG, PS, "Executing `%s`", object->data.name);

            PSObject dict_object;
            TRY(ps_interpreter_dict_entry(interpreter, object, &dict_object),
                "Couldn't find item to execute");

            if (dict_object.literal) {
                ps_interpreter_operand_push(interpreter, dict_object);
                break;
            }

            TRY(ps_object_execute(interpreter, &dict_object));
            break;
        }
        case PS_OBJECT_PROC: {
            LOG_DIAG(DEBUG, PS, "Executing procedure");

            for (size_t idx = 0; idx < ps_object_list_len(object->data.proc);
                 idx++) {
                PSObject proc_element;
                RELEASE_ASSERT(
                    ps_object_list_get(object->data.proc, idx, &proc_element)
                );

                TRY(ps_interpret_object(interpreter, proc_element));
            }

            break;
        }
        case PS_OBJECT_OPERATOR: {
            LOG_DIAG(TRACE, PS, "Executing operator");

            TRY(object->data.operator(interpreter),
                "Error occurred while executing operator");
            break;
        }
        default: {
            LOG_TODO("Execution of postscript type %d", (int)object->type);
        }
    }

    return NULL;
}

bool ps_object_eq(const PSObject* a, const PSObject* b) {
    RELEASE_ASSERT(a);
    RELEASE_ASSERT(b);

    if (a->type != b->type) {
        return false;
    }

    switch (a->type) {
        case PS_OBJECT_NAME: {
            return strcmp(a->data.name, b->data.name) == 0;
        }
        default: {
            LOG_TODO("Postscript equality for type %d", (int)a->type);
        }
    }

    return false;
}

const char* ps_object_fmt(Arena* arena, const PSObject* object) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(object);

    switch (object->type) {
        case PS_OBJECT_INTEGER: {
            return str_get_cstr(
                str_new_fmt(arena, "%d", (int)object->data.integer)
            );
        }
        case PS_OBJECT_NAME: {
            char* prefix = object->literal ? "/" : "";
            return str_get_cstr(
                str_new_fmt(arena, "%s%s", prefix, object->data.name)
            );
        }
        case PS_OBJECT_REAL: {
            return str_get_cstr(str_new_fmt(arena, "%f", object->data.real));
        }
        case PS_OBJECT_ARRAY: {
            char* delims = object->literal ? "[]" : "{}";

            Str* str = str_new_fmt(arena, "%c ", delims[0]);
            for (size_t idx = 0; idx < ps_object_list_len(object->data.array);
                 idx++) {
                PSObject* array_object = NULL;
                RELEASE_ASSERT(ps_object_list_get_ptr(
                    object->data.array,
                    idx,
                    &array_object
                ));

                str = str_new_fmt(
                    arena,
                    "%s%s ",
                    str_get_cstr(str),
                    ps_object_fmt(arena, array_object)
                );
            }

            return str_get_cstr(
                str_new_fmt(arena, "%s%c", str_get_cstr(str), delims[1])
            );
        }
        case PS_OBJECT_PROC: {
            Str* str = str_new_fmt(arena, "{ ");
            for (size_t idx = 0; idx < ps_object_list_len(object->data.proc);
                 idx++) {
                PSObject* proc_object = NULL;
                RELEASE_ASSERT(
                    ps_object_list_get_ptr(object->data.proc, idx, &proc_object)
                );

                str = str_new_fmt(
                    arena,
                    "%s%s ",
                    str_get_cstr(str),
                    ps_object_fmt(arena, proc_object)
                );
            }

            return str_get_cstr(str_new_fmt(arena, "%s}", str_get_cstr(str)));
        }
        case PS_OBJECT_OPERATOR: {
            return "<|builtin|>";
        }
        case PS_OBJECT_DICT: {
            Str* str = str_new_fmt(arena, "<< ");
            for (size_t idx = 0; idx < ps_object_list_len(object->data.dict);
                 idx++) {
                PSObject* dict_object = NULL;
                RELEASE_ASSERT(
                    ps_object_list_get_ptr(object->data.dict, idx, &dict_object)
                );

                str = str_new_fmt(
                    arena,
                    "%s%s ",
                    str_get_cstr(str),
                    ps_object_fmt(arena, dict_object)
                );
            }

            return str_get_cstr(str_new_fmt(arena, "%s>>", str_get_cstr(str)));
        }
        case PS_OBJECT_STRING: {
            return str_get_cstr(str_new_fmt(
                arena,
                "(%s)",
                ps_string_as_cstr(object->data.string, arena)
            ));
        }
        default: {
            LOG_TODO(
                "Print postscript object type %s",
                ps_object_name_lookup[object->type]
            );
        }
    }
}
