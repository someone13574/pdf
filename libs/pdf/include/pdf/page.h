#pragma once

#include "pdf/content_stream/stream.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf/resources.h"

typedef struct PdfPages PdfPages;
DESERDE_DECL_RESOLVABLE(
    PdfPagesRef,
    PdfPages,
    pdf_pages_ref_init,
    pdf_resolve_pages
)
DESERDE_DECL_OPTIONAL(PdfPagesRefOptional, PdfPagesRef, pdf_pages_ref_op_init)

typedef struct PdfPageTree PdfPageTree;
DESERDE_DECL_RESOLVABLE(
    PdfPageTreeRef,
    PdfPageTree,
    pdf_page_tree_ref_init,
    pdf_resolve_page_tree
)

#define DVEC_NAME PdfPageTreeRefVec
#define DVEC_LOWERCASE_NAME pdf_page_tree_ref_vec
#define DVEC_TYPE PdfPageTreeRef
#include "arena/dvec_decl.h"

typedef struct {
    /// (Required) The type of PDF object that this dictionary describes; shall
    /// be Page for a page object.
    PdfName type;

    /// (Required; shall be an indirect reference) The page tree node that is
    /// the immediate parent of this page object.
    PdfPagesRef parent;

    /// (Required if PieceInfo is present; optional otherwise; PDF 1.3) The date
    /// and time (see 7.9.4, "Dates") when the page’s contents were most
    /// recently modified. If a page-piece dictionary (PieceInfo) is present,
    /// the modification date shall be used to ascertain which of the
    /// application data dictionaries that it contains correspond to the current
    /// content of the page (see 14.5, "Page-Piece Dictionaries")
    PdfObject* last_modified;

    /// (Required; inheritable) A dictionary containing any resources required
    /// by the page (see 7.8.3, "Resource Dictionaries"). If the page requires
    /// no resources, the value of this entry shall be an empty dictionary.
    /// Omitting the entry entirely indicates that the resources shall be
    /// inherited from an ancestor node in the page tree.
    PdfResourcesOptional resources;

    /// (Required; inheritable) A rectangle (see 7.9.5, "Rectangles"), expressed
    /// in default user space units, that shall define the boundaries of the
    /// physical medium on which the page shall be displayed or printed
    /// (see 14.11.2, "Page Boundaries").
    PdfRectangleOptional media_box;

    /// (Optional; inheritable) A rectangle, expressed in default user space
    /// units, that shall define the visible region of default user space. When
    /// the page is displayed or printed, its contents shall be clipped
    /// (cropped) to this rectangle and then shall be imposed on the output
    /// medium in some implementation-defined manner (see 14.11.2, "Page
    /// Boundaries"). Default value: the value of MediaBox.
    PdfRectangleOptional crop_box;

    /// (Optional; PDF 1.3) A rectangle, expressed in default user space units,
    /// that shall define the region to which the contents of the page shall be
    /// clipped when output in a production environment (see 14.11.2, "Page
    /// Boundaries"). Default value: the value of CropBox.
    PdfRectangleOptional bleed_box;

    /// (Optional; PDF 1.3) A rectangle, expressed in default user space units,
    /// that shall define the intended dimensions of the finished page after
    /// trimming (see 14.11.2, "Page Boundaries"). Default value: the value of
    /// CropBox.
    PdfRectangleOptional trim_box;

    /// (Optional; PDF 1.3) A rectangle, expressed in default user space units,
    /// that shall define the extent of the page’s meaningful content (including
    /// potential white space) as intended by the page’s creator (see 14.11.2,
    /// "Page Boundaries"). Default value: the value of CropBox.
    PdfRectangleOptional art_box;

    /// (Optional; PDF 1.4) A box colour information dictionary that shall
    /// specify the colours and other visual characteristics that should be used
    /// in displaying guidelines on the screen for the various page boundaries
    /// (see 14.11.2.2, "Display of Page Boundaries"). If this entry is absent,
    /// the application shall use its own current default settings.
    PdfUnimplemented box_color_info;

    /// (Optional) A content stream (see 7.8.2, "Content Streams") that shall
    /// describe the contents of this page. If this entry is absent, the page
    /// shall be empty.
    PdfContentsStreamRefVecOptional contents;

    /// (Optional; inheritable) The number of degrees by which the page shall be
    /// rotated clockwise when displayed or printed. The value shall be a
    /// multiple of 90. Default value: 0.
    PdfIntegerOptional rotate;

    /// (Optional; PDF 1.4) A group attributes dictionary that shall specify the
    /// attributes of the page’s page group for use in the transparent imaging
    /// model (see 11.4.7, "Page Group" and 11.6.6, "Transparency Group
    /// XObjects").
    PdfIgnored group;

    /// (Optional) A stream object that shall define the page’s thumbnail image
    /// (see 12.3.4, "Thumbnail Images").
    PdfIgnored thumb;

    /// (Optional; PDF 1.1; recommended if the page contains article beads) An
    /// array that shall contain indirect references to all article beads
    /// appearing on the page (see 12.4.3, "Articles"). The beads shall be
    /// listed in the array in natural reading order.
    PdfIgnored b;

    /// (Optional; PDF 1.1) The page’s display duration (also called its advance
    /// timing): the maximum length of time, in seconds, that the page shall be
    /// displayed during presentations before the viewer application shall
    /// automatically advance to the next page (see 12.4.4, "Presentations"). By
    /// default, the viewer shall not advance automatically.
    PdfIgnored dur;

    /// (Optional; PDF 1.1) A transition dictionary describing the transition
    /// effect that shall be used when displaying the page during presentations
    /// (see 12.4.4, "Presentations").
    PdfIgnored trans;

    /// (Optional) An array of annotation dictionaries that shall contain
    /// indirect references to all annotations associated with the page
    /// (see 12.5, "Annotations").
    PdfIgnored annots;

    /// (Optional; PDF 1.2) An additional-actions dictionary that shall define
    /// actions to be performed when the page is opened or closed (see 12.6.3,
    /// "Trigger Events"). (PDF 1.3) additional-actions dictionaries are not
    /// inheritable.
    PdfIgnored aa;

    /// (Optional; PDF 1.4) A metadata stream that shall contain metadata for
    /// the page (see 14.3.2, "Metadata Streams").
    PdfIgnored metadata;

    /// (Optional; PDF 1.3) A page-piece dictionary associated with the page
    /// (see 14.5, "Page-Piece Dictionaries").
    PdfIgnored piece_info;

    /// (Required if the page contains structural content items; PDF 1.3) The
    /// integer key of the page’s entry in the structural parent tree
    /// (see 14.7.4.4, "Finding Structure Elements from Content Items").
    PdfUnimplemented struct_parents;

    /// (Optional; PDF 1.3; indirect reference preferred) The digital identifier
    /// of the page’s parent Web Capture content set (see 14.10.6, "Object
    /// Attributes Related to Web Capture").
    PdfIgnored id;

    /// (Optional; PDF 1.3) The page’s preferred zoom (magnification) factor:
    /// the factor by which it shall be scaled to achieve the natural display
    /// magnification (see 14.10.6, "Object Attributes Related to Web Capture").
    PdfIgnored pz;

    /// (Optional; PDF 1.3) A separation dictionary that shall contain
    /// information needed to generate colour separations for the page
    /// (see 14.11.4, "Separation Dictionaries").
    PdfUnimplemented separation_info;

    /// (Optional; PDF 1.5) A name specifying the tab order that shall be used
    /// for annotations on the page. The possible values shall be R (row order),
    /// C (column order), and S (structure order). See 12.5, "Annotations" for
    /// details.
    PdfIgnored tabs;

    /// (Required if this page was created from a named page object; PDF 1.5)
    /// The name of the originating page object (see 12.7.6, "Named Pages").
    PdfIgnored template_instantiated;

    /// (Optional; PDF 1.5) A navigation node dictionary that shall represent
    /// the first node on the page (see 12.4.4.2, "Sub-page Navigation").
    PdfIgnored pres_steps;

    /// (Optional; PDF 1.6) A positive number that shall give the size of
    /// default user space units, in multiples of 1/72 inch. The range of
    /// supported values shall be implementation-dependent. Default value: 1.0
    /// (user space unit is 1/72 inch).
    PdfUnimplemented user_unit;

    /// (Optional; PDF 1.6) An array of viewport dictionaries (see Table 260)
    /// that shall specify rectangular regions of the page.
    PdfUnimplemented vp;
} PdfPage;

Error* pdf_deserialize_page(
    const PdfObject* object,
    PdfPage* target_ptr,
    PdfResolver* resolver
);

DESERDE_DECL_RESOLVABLE(
    PdfPageRef,
    PdfPage,
    pdf_page_ref_init,
    pdf_resolve_page
)

struct PdfPages {
    /// (Required) The type of PDF object that this dictionary describes; shall
    /// be Pages for a page tree node.
    PdfName type;

    /// (Required except in root node; prohibited in the root node; shall be an
    /// indirect reference) The page tree node that is the immediate parent of
    /// this one.
    PdfPagesRefOptional parent;

    /// (Required) An array of indirect references to the immediate children of
    /// this node. The children shall only be page objects or other page tree
    /// nodes.
    PdfPageTreeRefVec* kids;

    /// (Required) The number of leaf nodes (page objects) that are descendants
    /// of this node within the page tree.
    PdfInteger count;

    /// (Required; inheritable) A dictionary containing any resources required
    /// by the page (see 7.8.3, "Resource Dictionaries"). If the page requires
    /// no resources, the value of this entry shall be an empty dictionary.
    /// Omitting the entry entirely indicates that the resources shall be
    /// inherited from an ancestor node in the page tree.
    PdfResourcesOptional resources;

    /// (Required; inheritable) A rectangle (see 7.9.5, "Rectangles"), expressed
    /// in default user space units, that shall define the boundaries of the
    /// physical medium on which the page shall be displayed or printed
    /// (see 14.11.2, "Page Boundaries").
    PdfRectangleOptional media_box;

    /// (Optional; inheritable) A rectangle, expressed in default user space
    /// units, that shall define the visible region of default user space. When
    /// the page is displayed or printed, its contents shall be clipped
    /// (cropped) to this rectangle and then shall be imposed on the output
    /// medium in some implementation-defined manner (see 14.11.2, "Page
    /// Boundaries"). Default value: the value of MediaBox.
    PdfRectangleOptional crop_box;

    /// (Optional; inheritable) The number of degrees by which the page shall be
    /// rotated clockwise when displayed or printed. The value shall be a
    /// multiple of 90. Default value: 0.
    PdfIntegerOptional rotate;
};

Error* pdf_deserialize_pages(
    const PdfObject* object,
    PdfPages* target_ptr,
    PdfResolver* resolver
);

struct PdfPageTree {
    enum { PDF_PAGE_TREE_PAGE, PDF_PAGE_TREE_PAGES } kind;
    union {
        PdfPage page;
        PdfPages pages;
    } value;
};

Error* pdf_deserialize_page_tree(
    const PdfObject* object,
    PdfPageTree* target_ptr,
    PdfResolver* resolver
);

/// Fills inheritable properties in dst from src if they are not already set
void pdf_page_tree_inherit(PdfPageTree* dst, PdfPages* src);

typedef struct PdfPageIter PdfPageIter;

Error* pdf_page_iter_new(
    PdfResolver* resolver,
    PdfPagesRef root_ref,
    PdfPageIter** iter_out
);

Error* pdf_page_iter_next(PdfPageIter* iter, PdfPage* out_page, bool* done);
