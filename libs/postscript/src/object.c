#include "postscript/object.h"

#include <string.h>

#include "logger/log.h"
#include "pdf_error/error.h"
#include "postscript/interpreter.h"

#define DLINKED_NAME PostscriptObjectList
#define DLINKED_LOWERCASE_NAME postscript_object_list
#define DLINKED_TYPE PostscriptObject
#include "arena/dlinked_impl.h"

PdfError* postscript_object_execute(
    PostscriptInterpreter* interpreter,
    const PostscriptObject* object
) {
    RELEASE_ASSERT(interpreter);
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(!object->literal);

    switch (object->type) {
        case POSTSCRIPT_OBJECT_NAME: {
            PostscriptObject dict_object;
            PDF_PROPAGATE(postscript_interpreter_dict_entry(
                interpreter,
                object,
                &dict_object
            ));

            PDF_PROPAGATE(postscript_object_execute(interpreter, &dict_object));
            break;
        }
        case POSTSCRIPT_OBJECT_CUSTOM_PROC: {
            PDF_PROPAGATE(
                object->data.custom_proc(interpreter),
                "Error occurred while executing procedure"
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

char* postscript_object_fmt(Arena* arena, const PostscriptObject* object) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(object);

    switch (object->type) {
        case POSTSCRIPT_OBJECT_NAME: {
            return object->data.name;
        }
        default: {
            LOG_TODO("Print postscript object type %d", (int)object->type);
        }
    }
}
