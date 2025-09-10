#include "operators.h"

#include <stdbool.h>

#include "arena/arena.h"
#include "logger/log.h"
#include "pdf_error/error.h"
#include "postscript/interpreter.h"
#include "postscript/object.h"
#include "postscript/resource.h"

static void push_operator(
    PostscriptObjectList* dict,
    PostscriptOperator
    operator,
    char * name
) {
    RELEASE_ASSERT(dict);
    RELEASE_ASSERT(operator);
    RELEASE_ASSERT(name);

    LOG_DIAG(DEBUG, PS, "Adding systemdict operator `%s`", name);

    postscript_object_list_push_back(
        dict,
        (PostscriptObject
        ) {.type = POSTSCRIPT_OBJECT_NAME,
           .data.name = name,
           .access = POSTSCRIPT_ACCESS_UNLIMITED,
           .literal = true}
    );

    postscript_object_list_push_back(
        dict,
        (PostscriptObject
        ) {.type = POSTSCRIPT_OBJECT_OPERATOR,
           .data.operator= operator,
           .access = POSTSCRIPT_ACCESS_EXECUTE_ONLY,
           .literal = false}
    );
}

PostscriptObject postscript_systemdict_ops(Arena* arena) {
    RELEASE_ASSERT(arena);

    LOG_DIAG(INFO, PS, "Getting systemdict operators");

    PostscriptObject dict = {
        .type = POSTSCRIPT_OBJECT_DICT,
        .data.dict = postscript_object_list_new(arena),
        .access = POSTSCRIPT_ACCESS_READ_ONLY,
        .literal = true
    };

    push_operator(dict.data.dict, postscript_op_pop, "pop");
    push_operator(dict.data.dict, postscript_op_dup, "dup");
    push_operator(dict.data.dict, postscript_op_dict, "dict");
    push_operator(dict.data.dict, postscript_op_def, "def");
    push_operator(dict.data.dict, postscript_op_begin, "begin");
    push_operator(dict.data.dict, postscript_op_end, "end");
    push_operator(dict.data.dict, postscript_op_currentdict, "currentdict");
    push_operator(dict.data.dict, postscript_op_findresource, "findresource");
    push_operator(
        dict.data.dict,
        postscript_op_defineresource,
        "defineresource"
    );

    return dict;
}

PdfError* postscript_op_pop(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PostscriptObject popped;
    PDF_PROPAGATE(postscript_interpreter_pop_operand(interpreter, &popped));

    return NULL;
}

PdfError* postscript_op_dup(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PostscriptObject object;
    PDF_PROPAGATE(postscript_interpreter_pop_operand(interpreter, &object));

    postscript_interpreter_operand_push(interpreter, object);
    postscript_interpreter_operand_push(interpreter, object);

    return NULL;
}

PdfError* postscript_op_dict(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PostscriptObject length_object;
    PDF_PROPAGATE(postscript_interpreter_pop_operand_typed(
        interpreter,
        POSTSCRIPT_OBJECT_INTEGER,
        true,
        &length_object
    ));

    postscript_interpreter_operand_push(
        interpreter,
        (PostscriptObject
        ) {.type = POSTSCRIPT_OBJECT_DICT,
           .data.dict = postscript_object_list_new(
               postscript_interpreter_get_arena(interpreter)
           ),
           .access = POSTSCRIPT_ACCESS_UNLIMITED,
           .literal = true}
    );

    return NULL;
}

PdfError* postscript_op_def(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PostscriptObject value;
    PostscriptObject key;
    PDF_PROPAGATE(postscript_interpreter_pop_operand(interpreter, &value));
    PDF_PROPAGATE(postscript_interpreter_pop_operand(interpreter, &key));

    PDF_PROPAGATE(postscript_interpreter_define(interpreter, key, value));

    return NULL;
}

PdfError* postscript_op_begin(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PostscriptObject dict_object;
    PDF_PROPAGATE(postscript_interpreter_pop_operand_typed(
        interpreter,
        POSTSCRIPT_OBJECT_DICT,
        true,
        &dict_object
    ));
    postscript_interpreter_dict_push(interpreter, dict_object);

    return NULL;
}

PdfError* postscript_op_end(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PDF_PROPAGATE(postscript_interpreter_dict_pop(interpreter));

    return NULL;
}

PdfError* postscript_op_currentdict(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PostscriptObject current_dict;
    postscript_interpreter_dict(interpreter, &current_dict);
    postscript_interpreter_operand_push(interpreter, current_dict);

    return NULL;
}

PdfError* postscript_op_defineresource(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PostscriptObject category_name;
    PostscriptObject instance;
    PostscriptObject key_name;
    PDF_PROPAGATE(postscript_interpreter_pop_operand_typed(
        interpreter,
        POSTSCRIPT_OBJECT_NAME,
        true,
        &category_name
    ));
    PDF_PROPAGATE(postscript_interpreter_pop_operand_typed(
        interpreter,
        POSTSCRIPT_OBJECT_DICT,
        true,
        &instance
    ));
    PDF_PROPAGATE(postscript_interpreter_pop_operand_typed(
        interpreter,
        POSTSCRIPT_OBJECT_NAME,
        true,
        &key_name
    ));

    PostscriptResourceCategory* category = postscript_get_resource_category(
        postscript_interpreter_get_resource_categories(interpreter),
        category_name.data.name
    );
    if (!category) {
        return PDF_ERROR(
            PDF_ERR_POSTSCRIPT_UNKNOWN_RESOURCE,
            "Unknown resource category %s",
            category_name.data.name
        );
    }

    PostscriptResource resource =
        postscript_resource_new(key_name.data.name, instance);
    postscript_resource_category_add_resource(category, resource);

    postscript_interpreter_operand_push(interpreter, instance);

    return NULL;
}

PdfError* postscript_op_findresource(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    // Read operands
    PostscriptObject category_name_object;
    PDF_PROPAGATE(postscript_interpreter_pop_operand_typed(
        interpreter,
        POSTSCRIPT_OBJECT_NAME,
        true,
        &category_name_object
    ));

    PostscriptObject resource_name_object;
    PDF_PROPAGATE(postscript_interpreter_pop_operand_typed(
        interpreter,
        POSTSCRIPT_OBJECT_NAME,
        true,
        &resource_name_object
    ));

    // Lookup resource
    PostscriptResourceCategory* category = postscript_get_resource_category(
        postscript_interpreter_get_resource_categories(interpreter),
        category_name_object.data.name
    );
    if (!category) {
        return PDF_ERROR(
            PDF_ERR_POSTSCRIPT_UNKNOWN_RESOURCE,
            "Unknown resource category for resource %s/%s",
            category_name_object.data.name,
            resource_name_object.data.name
        );
    }

    PostscriptResource* resource = postscript_resource_category_get_resource(
        category,
        resource_name_object.data.name
    );
    if (!resource) {
        return PDF_ERROR(
            PDF_ERR_POSTSCRIPT_UNKNOWN_RESOURCE,
            "Unknown resource %s in category %s",
            resource_name_object.data.name,
            category_name_object.data.name
        );
    }

    // Push resource to operand stack
    postscript_interpreter_operand_push(interpreter, resource->object);

    return NULL;
}
