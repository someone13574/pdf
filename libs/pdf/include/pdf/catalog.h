#pragma once

#include "err/error.h"
#include "pdf/object.h"
#include "pdf/page.h"
#include "pdf/resolver.h"

// Catalog
typedef struct {
    PdfName type;
    PdfUnimplemented version;
    PdfUnimplemented extensions;
    PdfPagesRef pages;
    PdfUnimplemented page_labels;
    PdfUnimplemented names;
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
    PdfUnimplemented struct_tree_root;
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
} PdfCatalog;

Error* pdf_deser_catalog(
    const PdfObject* object,
    PdfCatalog* target_ptr,
    PdfResolver* resolver
);

DESER_DECL_RESOLVABLE(
    PdfCatalogRef,
    PdfCatalog,
    pdf_catalog_ref_init,
    pdf_resolve_catalog
)
