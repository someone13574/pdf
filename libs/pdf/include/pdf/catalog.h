#pragma once

#include "err/error.h"
#include "pdf/deserde.h"
#include "pdf/object.h"
#include "pdf/page.h"
#include "pdf/resolver.h"

// Catalog
typedef struct {
    PdfName type;
    PdfUnimplemented version;
    PdfUnimplemented extensions;
    PdfPagesRef pages;
    PdfIgnored page_labels;
    PdfIgnored names;
    PdfIgnored dests;
    PdfIgnored viewer_preferences;
    PdfIgnored page_layout;
    PdfIgnored page_mode;
    PdfIgnored outlines;
    PdfIgnored threads;
    PdfIgnored open_action;
    PdfIgnored aa;
    PdfIgnored uri;
    PdfIgnored acro_form;
    PdfIgnored metadata;
    PdfIgnored struct_tree_root;
    PdfIgnored mark_info;
    PdfIgnored lang;
    PdfIgnored spider_info;
    PdfUnimplemented output_intents;
    PdfIgnored piece_info;
    PdfIgnored oc_properties;
    PdfUnimplemented perms;
    PdfIgnored legal;
    PdfUnimplemented requirements;
    PdfUnimplemented collection;
    PdfIgnored needs_rendering;

    /// An array of one or more file specification dictionaries which denote
    /// the associated files for this PDF document. (PDF 2.0)
    PdfIgnored af;
} PdfCatalog;

Error* pdf_deserde_catalog(
    const PdfObject* object,
    PdfCatalog* target_ptr,
    PdfResolver* resolver
);

PDF_DECL_RESOLVABLE_FIELD(PdfCatalog, PdfCatalogRef, catalog)
