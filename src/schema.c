#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "arena.h"
#include "ctx.h"
#include "object.h"
#include "pdf_object.h"
#include "pdf_result.h"
#include "pdf_schema.h"
#include "schema_macros.h"

// Page tree node
SCHEMA_IMPL(
    PdfSchemaPageTreeNode,
    page_tree_node,
    schema_validation_none, // No easy way to validate that root doesn't have
                            // Parent key
    (REQUIRED, NAME, type, "Type"),
    (OPTIONAL_SCHEMA_REF, page_tree_node, parent, "Parent"),
    (REQUIRED, ARRAY, kids, "Kids"),
    (REQUIRED, INTEGER, count, "Count")
)
SCHEMA_IMPL_GET_REF(PdfSchemaPageTreeNode, page_tree_node)
SCHEMA_IMPL_GET_OPTIONAL_REF(PdfSchemaPageTreeNode, page_tree_node)

// Catalog
bool validate_catalog_schema(PdfSchemaCatalog* catalog, PdfResult* result) {
    if (strcmp(catalog->type, "Catalog") != 0) {
        *result = PDF_ERR_SCHEMA_INCORRECT_TYPE_NAME;
        return false;
    }

    return true;
}

SCHEMA_IMPL(
    PdfSchemaCatalog,
    catalog,
    validate_catalog_schema,
    (REQUIRED, NAME, type, "Type"),
    (REQUIRED_SCHEMA_REF, page_tree_node, pages, "Pages")
)
SCHEMA_IMPL_GET_REF(PdfSchemaCatalog, catalog)

// Trailer
SCHEMA_IMPL(
    PdfSchemaTrailer,
    trailer,
    schema_validation_none,
    (REQUIRED, INTEGER, size, "Size"),
    (REQUIRED_SCHEMA_REF, catalog, root, "Root")
)

#ifdef TEST

#include "test.h"

TEST_FUNC(test_trailer_schema) {
    char buffer[] = "<</Size 5 /Root 1 0 R >>";
    Arena* arena = arena_new(128);
    PdfCtx* ctx = pdf_ctx_new(arena, buffer, strlen(buffer));
    PdfResult result = PDF_OK;
    PdfObject* object = pdf_parse_object(arena, ctx, &result, false);
    TEST_ASSERT_EQ((PdfResult)PDF_OK, result);
    TEST_ASSERT(object);

    PdfSchemaTrailer* trailer = pdf_schema_trailer_new(arena, object, &result);
    TEST_ASSERT_EQ((PdfResult)PDF_OK, result);
    TEST_ASSERT(trailer);

    TEST_ASSERT_EQ(trailer->size, 5);
    TEST_ASSERT_EQ(
        (uintptr_t)trailer->root.get,
        (uintptr_t)pdf_schema_catalog_get_ref
    );
    TEST_ASSERT_EQ(trailer->root.ref.object_id, (size_t)1);
    TEST_ASSERT_EQ(trailer->root.ref.generation, (size_t)0);
    TEST_ASSERT(!trailer->root.cached);

    return TEST_RESULT_PASS;
}

#endif // TEST
