#include "pdf/catalog.h"

#include "deserialize.h"
#include "logger/log.h"
#include "pdf/object.h"
#include "pdf/page.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"

PdfError* pdf_deserialize_catalog(
    const PdfObject* object,
    PdfCatalog* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            "Type",
            &target_ptr->type,
            PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(
            "Pages",
            &target_ptr->pages,
            PDF_DESERDE_RESOLVABLE(pdf_page_tree_node_ref_init)
        ),
        PDF_IGNORED_FIELD("PageMode"),
        PDF_IGNORED_FIELD("OpenAction")
    };

    PDF_PROPAGATE(pdf_deserialize_dict(
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        false,
        resolver,
        "PdfCatalog"
    ));

    return NULL;
}

DESERDE_IMPL_RESOLVABLE(
    PdfCatalogRef,
    PdfCatalog,
    pdf_catalog_ref_init,
    pdf_resolve_catalog,
    pdf_deserialize_catalog
)
