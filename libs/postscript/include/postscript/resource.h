#pragma once

#include "arena/arena.h"
#include "pdf_error/error.h"
#include "postscript/object.h"

/// A collection of named objects that reside in VM or can be located and
/// brought into VM on demand.
typedef struct {
    char* name;
    PostscriptObject object;
} PostscriptResource;

/// Creates a new resource from an existing object.
PostscriptResource postscript_resource_new(char* name, PostscriptObject object);

/// Create a new resource with dictionary storage.
PostscriptResource postscript_resource_new_dict(Arena* arena, char* name);

/// Add a new operator to the dictionary resource.
void postscript_resource_add_op(
    PostscriptResource* resource,
    PostscriptOperator
    operator,
    char * name
);

#define DVEC_NAME PostscriptResourceVec
#define DVEC_LOWERCASE_NAME postscript_resource_vec
#define DVEC_TYPE PostscriptResource
#include "arena/dvec_decl.h"

/// A collection of named resources.
typedef struct {
    char* name;
    PostscriptResourceVec* resources;
} PostscriptResourceCategory;

/// Create a new resource category.
PostscriptResourceCategory
postscript_resource_category_new(Arena* arena, char* name);

/// Add a resource to a resource category
PdfError* postscript_resource_category_add_resource(
    PostscriptResourceCategory* category,
    PostscriptResource resource
);

#define DVEC_NAME PostscriptResourceCategoryVec
#define DVEC_LOWERCASE_NAME postscript_resource_category_vec
#define DVEC_TYPE PostscriptResourceCategory
#include "arena/dvec_decl.h"

/// Get a resource category from a list. Returns NULL if it doesn't exist.
PostscriptResourceCategory* postscript_get_resource_category(
    const PostscriptResourceCategoryVec* resource_categories,
    char* name
);

/// Get a resource from a resource category. Returns NULL if it doesn't exist.
PostscriptResource* postscript_resource_category_get_resource(
    const PostscriptResourceCategory* resource_category,
    char* name
);
