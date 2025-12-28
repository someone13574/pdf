#include "postscript/resource.h"

#include <string.h>

#include "logger/log.h"
#include "pdf_error/error.h"
#include "postscript/object.h"

#define DVEC_NAME PostscriptResourceVec
#define DVEC_LOWERCASE_NAME postscript_resource_vec
#define DVEC_TYPE PostscriptResource
#include "arena/dvec_impl.h"

#define DVEC_NAME PostscriptResourceCategoryVec
#define DVEC_LOWERCASE_NAME postscript_resource_category_vec
#define DVEC_TYPE PostscriptResourceCategory
#include "arena/dvec_impl.h"

PostscriptResource
postscript_resource_new(char* name, PostscriptObject object) {
    RELEASE_ASSERT(name);

    return (PostscriptResource) {.name = name, .object = object};
}

PostscriptResource postscript_resource_new_dict(Arena* arena, char* name) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(name);

    return (PostscriptResource) {.name = name,
                                 .object.type = POSTSCRIPT_OBJECT_DICT,
                                 .object.data.dict =
                                     postscript_object_list_new(arena),
                                 .object.access = POSTSCRIPT_ACCESS_READ_ONLY,
                                 .object.literal = true};
}

void postscript_resource_add_op(
    PostscriptResource* resource,
    PostscriptOperator operator,
    char* name
) {
    RELEASE_ASSERT(resource);
    RELEASE_ASSERT(name);
    RELEASE_ASSERT(operator);
    RELEASE_ASSERT(resource->object.type == POSTSCRIPT_OBJECT_DICT);

    postscript_object_list_push_back(
        resource->object.data.dict,
        (PostscriptObject) {.type = POSTSCRIPT_OBJECT_NAME,
                            .data.name = name,
                            .access = POSTSCRIPT_ACCESS_UNLIMITED,
                            .literal = true}
    );

    postscript_object_list_push_back(
        resource->object.data.dict,
        (PostscriptObject) {.type = POSTSCRIPT_OBJECT_OPERATOR,
                            .data.operator = operator,
                            .access = POSTSCRIPT_ACCESS_EXECUTE_ONLY,
                            .literal = false}
    );
}

PostscriptResourceCategory
postscript_resource_category_new(Arena* arena, char* name) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(name);

    return (
        PostscriptResourceCategory
    ) {.name = name, .resources = postscript_resource_vec_new(arena)};
}

PdfError* postscript_resource_category_add_resource(
    PostscriptResourceCategory* category,
    PostscriptResource resource
) {
    RELEASE_ASSERT(category);
    RELEASE_ASSERT(resource.name);

    if (postscript_resource_category_get_resource(category, resource.name)) {
        PDF_ERROR(
            PDF_ERR_POSTSCRIPT_RESOURCE_DEFINED,
            "Resource `%s` is already defined",
            resource.name
        );
    }

    postscript_resource_vec_push(category->resources, resource);

    return NULL;
}

PostscriptResourceCategory* postscript_get_resource_category(
    const PostscriptResourceCategoryVec* resource_categories,
    char* name
) {
    RELEASE_ASSERT(resource_categories);

    for (size_t idx = 0;
         idx < postscript_resource_category_vec_len(resource_categories);
         idx++) {
        PostscriptResourceCategory* category;
        RELEASE_ASSERT(postscript_resource_category_vec_get_ptr(
            resource_categories,
            idx,
            &category
        ));

        if (strcmp(name, category->name) == 0) {
            return category;
        }
    }

    return NULL;
}

PostscriptResource* postscript_resource_category_get_resource(
    const PostscriptResourceCategory* resource_category,
    char* name
) {
    RELEASE_ASSERT(resource_category);

    for (size_t idx = 0;
         idx < postscript_resource_vec_len(resource_category->resources);
         idx++) {
        PostscriptResource* resource;
        RELEASE_ASSERT(postscript_resource_vec_get_ptr(
            resource_category->resources,
            idx,
            &resource
        ));

        if (strcmp(name, resource->name) == 0) {
            return resource;
        }
    }

    return NULL;
}
