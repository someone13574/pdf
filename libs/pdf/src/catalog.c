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
        PDF_UNIMPLEMENTED_FIELD("Version"),
        PDF_UNIMPLEMENTED_FIELD("Extensions"),
        PDF_FIELD(
            "Pages",
            &target_ptr->pages,
            PDF_DESERDE_RESOLVABLE(pdf_pages_ref_init)
        ),
        PDF_UNIMPLEMENTED_FIELD("PageLabels"),
        PDF_UNIMPLEMENTED_FIELD("Names"),
        PDF_IGNORED_FIELD("Dests", &target_ptr->dests),
        PDF_IGNORED_FIELD("ViewerPreferences", &target_ptr->viewer_preferences),
        PDF_IGNORED_FIELD("PageLayout", &target_ptr->page_layout),
        PDF_IGNORED_FIELD("PageMode", &target_ptr->page_mode),
        PDF_IGNORED_FIELD("Outlines", &target_ptr->outlines),
        PDF_IGNORED_FIELD("Threads", &target_ptr->threads),
        PDF_IGNORED_FIELD("OpenAction", &target_ptr->open_action),
        PDF_IGNORED_FIELD("AA", &target_ptr->aa),
        PDF_IGNORED_FIELD("URI", &target_ptr->uri),
        PDF_IGNORED_FIELD("AcroForm", &target_ptr->acro_form),
        PDF_IGNORED_FIELD("Metadata", &target_ptr->metadata),
        PDF_UNIMPLEMENTED_FIELD("StructTreeRoot"),
        PDF_IGNORED_FIELD("MarkInfo", &target_ptr->mark_info),
        PDF_IGNORED_FIELD("Lang", &target_ptr->lang),
        PDF_IGNORED_FIELD("SpiderInfo", &target_ptr->spider_info),
        PDF_UNIMPLEMENTED_FIELD("OutputIntents"),
        PDF_IGNORED_FIELD("PieceInfo", &target_ptr->piece_info),
        PDF_IGNORED_FIELD(
            "OCProperties",
            &target_ptr->oc_properties
        ), // TODO: Optional content
        PDF_UNIMPLEMENTED_FIELD("Perms"),
        PDF_IGNORED_FIELD("Legal", &target_ptr->legal),
        PDF_UNIMPLEMENTED_FIELD("Requirements"),
        PDF_UNIMPLEMENTED_FIELD("Collection"),
        PDF_IGNORED_FIELD("NeedsRendering", &target_ptr->needs_rendering),
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
