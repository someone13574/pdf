#include "postscript/resource.h"

#include <string.h>

#include "logger/log.h"
#include "pdf_error/error.h"
#include "postscript/object.h"

#define DVEC_NAME PSResourceVec
#define DVEC_LOWERCASE_NAME ps_resource_vec
#define DVEC_TYPE PSResource
#include "arena/dvec_impl.h"

#define DVEC_NAME PSResourceCategoryVec
#define DVEC_LOWERCASE_NAME ps_resource_category_vec
#define DVEC_TYPE PSResourceCategory
#include "arena/dvec_impl.h"

PSResource ps_resource_new(char* name, PSObject object) {
    RELEASE_ASSERT(name);

    return (PSResource) {.name = name, .object = object};
}

PSResource ps_resource_new_dict(Arena* arena, char* name) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(name);

    return (PSResource) {.name = name,
                         .object.type = PS_OBJECT_DICT,
                         .object.data.dict = ps_object_list_new(arena),
                         .object.access = PS_ACCESS_READ_ONLY,
                         .object.literal = true};
}

void ps_resource_add_op(PSResource* resource, PSOperator operator, char* name) {
    RELEASE_ASSERT(resource);
    RELEASE_ASSERT(name);
    RELEASE_ASSERT(operator);
    RELEASE_ASSERT(resource->object.type == PS_OBJECT_DICT);

    ps_object_list_push_back(
        resource->object.data.dict,
        (PSObject) {.type = PS_OBJECT_NAME,
                    .data.name = name,
                    .access = PS_ACCESS_UNLIMITED,
                    .literal = true}
    );

    ps_object_list_push_back(
        resource->object.data.dict,
        (PSObject) {.type = PS_OBJECT_OPERATOR,
                    .data.operator = operator,
                    .access = PS_ACCESS_EXECUTE_ONLY,
                    .literal = false}
    );
}

PSResourceCategory ps_resource_category_new(Arena* arena, char* name) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(name);

    return (PSResourceCategory) {.name = name,
                                 .resources = ps_resource_vec_new(arena)};
}

PdfError* ps_resource_category_add_resource(
    PSResourceCategory* category,
    PSResource resource
) {
    RELEASE_ASSERT(category);
    RELEASE_ASSERT(resource.name);

    if (ps_resource_category_get_resource(category, resource.name)) {
        PDF_ERROR(
            PS_ERR_RESOURCE_DEFINED,
            "Resource `%s` is already defined",
            resource.name
        );
    }

    ps_resource_vec_push(category->resources, resource);

    return NULL;
}

PSResourceCategory* ps_get_resource_category(
    const PSResourceCategoryVec* resource_categories,
    char* name
) {
    RELEASE_ASSERT(resource_categories);

    for (size_t idx = 0;
         idx < ps_resource_category_vec_len(resource_categories);
         idx++) {
        PSResourceCategory* category;
        RELEASE_ASSERT(ps_resource_category_vec_get_ptr(
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

PSResource* ps_resource_category_get_resource(
    const PSResourceCategory* resource_category,
    char* name
) {
    RELEASE_ASSERT(resource_category);

    for (size_t idx = 0;
         idx < ps_resource_vec_len(resource_category->resources);
         idx++) {
        PSResource* resource;
        RELEASE_ASSERT(ps_resource_vec_get_ptr(
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
