#pragma once

#include "arena/arena.h"
#include "pdf_error/error.h"
#include "postscript/object.h"

/// A collection of named objects that reside in VM or can be located and
/// brought into VM on demand.
typedef struct {
    char* name;
    PSObject object;
} PSResource;

/// Creates a new resource from an existing object.
PSResource ps_resource_new(char* name, PSObject object);

/// Create a new resource with dictionary storage.
PSResource ps_resource_new_dict(Arena* arena, char* name);

/// Add a new operator to the dictionary resource.
void ps_resource_add_op(PSResource* resource, PSOperator operator, char * name);

#define DVEC_NAME PSResourceVec
#define DVEC_LOWERCASE_NAME ps_resource_vec
#define DVEC_TYPE PSResource
#include "arena/dvec_decl.h"

/// A collection of named resources.
typedef struct {
    char* name;
    PSResourceVec* resources;
} PSResourceCategory;

/// Create a new resource category.
PSResourceCategory ps_resource_category_new(Arena* arena, char* name);

/// Add a resource to a resource category
PdfError* ps_resource_category_add_resource(
    PSResourceCategory* category,
    PSResource resource
);

#define DVEC_NAME PSResourceCategoryVec
#define DVEC_LOWERCASE_NAME ps_resource_category_vec
#define DVEC_TYPE PSResourceCategory
#include "arena/dvec_decl.h"

/// Get a resource category from a list. Returns NULL if it doesn't exist.
PSResourceCategory* ps_get_resource_category(
    const PSResourceCategoryVec* resource_categories,
    char* name
);

/// Get a resource from a resource category. Returns NULL if it doesn't exist.
PSResource* ps_resource_category_get_resource(
    const PSResourceCategory* resource_category,
    char* name
);
