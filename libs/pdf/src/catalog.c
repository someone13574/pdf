#include "pdf/catalog.h"

#include "err/error.h"
#include "logger/log.h"
#include "pdf/deserde.h"
#include "pdf/object.h"
#include "pdf/page.h"
#include "pdf/resolver.h"
#include "pdf/types.h"

Error* pdf_deserde_catalog(
    const PdfObject* object,
    PdfCatalog* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    PdfFieldDescriptor fields[] = {
        pdf_name_field("Type", &target_ptr->type),
        pdf_unimplemented_field("Version"),
        pdf_unimplemented_field("Extensions"),
        pdf_pages_ref_field("Pages", &target_ptr->pages),
        pdf_unimplemented_field("PageLabels"),
        pdf_unimplemented_field("Names"),
        pdf_ignored_field("Dests", &target_ptr->dests),
        pdf_ignored_field("ViewerPreferences", &target_ptr->viewer_preferences),
        pdf_ignored_field("PageLayout", &target_ptr->page_layout),
        pdf_ignored_field("PageMode", &target_ptr->page_mode),
        pdf_ignored_field("Outlines", &target_ptr->outlines),
        pdf_ignored_field("Threads", &target_ptr->threads),
        pdf_ignored_field("OpenAction", &target_ptr->open_action),
        pdf_ignored_field("AA", &target_ptr->aa),
        pdf_ignored_field("URI", &target_ptr->uri),
        pdf_ignored_field("AcroForm", &target_ptr->acro_form),
        pdf_ignored_field("Metadata", &target_ptr->metadata),
        pdf_unimplemented_field("StructTreeRoot"),
        pdf_ignored_field("MarkInfo", &target_ptr->mark_info),
        pdf_ignored_field("Lang", &target_ptr->lang),
        pdf_ignored_field("SpiderInfo", &target_ptr->spider_info),
        pdf_unimplemented_field("OutputIntents"),
        pdf_ignored_field("PieceInfo", &target_ptr->piece_info),
        pdf_ignored_field(
            "OCProperties",
            &target_ptr->oc_properties
        ), // TODO: Optional content
        pdf_unimplemented_field("Perms"),
        pdf_ignored_field("Legal", &target_ptr->legal),
        pdf_unimplemented_field("Requirements"),
        pdf_unimplemented_field("Collection"),
        pdf_ignored_field("NeedsRendering", &target_ptr->needs_rendering),
    };

    TRY(pdf_deserde_fields(
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        false,
        resolver,
        "PdfCatalog"
    ));

    return NULL;
}

PDF_IMPL_RESOLVABLE_FIELD(PdfCatalog, PdfCatalogRef, catalog)
