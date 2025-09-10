#include "procedures.h"

#include <stdbool.h>

#include "arena/arena.h"
// #include "interpreter.h"
#include "logger/log.h"
#include "pdf_error/error.h"
#include "postscript/interpreter.h"
#include "postscript/object.h"
#include "postscript/resource.h"

static void push_procedure(
    PostscriptObjectList* dict,
    PostscriptCustomProcedure procedure,
    char* name
) {
    RELEASE_ASSERT(dict);
    RELEASE_ASSERT(procedure);
    RELEASE_ASSERT(name);

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
        ) {.type = POSTSCRIPT_OBJECT_CUSTOM_PROC,
           .data.custom_proc = procedure,
           .access = POSTSCRIPT_ACCESS_EXECUTE_ONLY,
           .literal = false}
    );
}

PostscriptObject postscript_systemdict_procedures(Arena* arena) {
    PostscriptObject dict = {
        .type = POSTSCRIPT_OBJECT_DICT,
        .data.dict = postscript_object_list_new(arena),
        .access = POSTSCRIPT_ACCESS_EXECUTE_ONLY,
        .literal = true
    };

    push_procedure(dict.data.dict, postscript_proc_begin, "begin");
    push_procedure(
        dict.data.dict,
        postscript_proc_findresource,
        "findresource"
    );

    return dict;
}

PdfError* postscript_proc_begin(PostscriptInterpreter* interpreter) {
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

PdfError* postscript_proc_findresource(PostscriptInterpreter* interpreter) {
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
