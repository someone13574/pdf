#pragma once

#include "pdf/content_stream.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf/resources.h"

typedef struct {
    /// The type of PDF object that this dictionary describes; if present, shall
    /// be XObject for a form XObject.
    PdfNameOptional type;

    /// The type of XObject that this dictionary describes; shall be Form for a
    /// form XObject.
    PdfName subtype;

    /// A code identifying the type of form XObject that this dictionary
    /// describes. The only valid value is 1. Default value: 1.
    PdfIntegerOptional form_type;

    /// An array of four numbers in the form coordinate system (see above),
    /// giving the coordinates of the left, bottom, right, and top edges,
    /// respectively, of the form XObject’s bounding box. These boundaries shall
    /// be used to clip the form XObject and to determine its size for caching.
    PdfRectangle bbox;

    /// An array of six numbers specifying the form matrix, which maps form
    /// space into user space (see 8.3.4, "Transformation Matrices"). Default
    /// value: the identity matrix [ 1 0 0 1 0 0 ].
    PdfUnimplemented matrix;

    /// A dictionary specifying any resources (such as fonts and images)
    /// required by the form XObject (see 7.8, "Content Streams and Resources").
    PdfResourcesOptional resources;

    /// A group attributes dictionary indicating that the contents of the form
    /// XObject shall be treated as a group and specifying the attributes of
    /// that group (see 8.10.3, "Group XObjects"). If a Ref entry (see below) is
    /// present, the group attributes shall also apply to the external page
    /// imported by that entry, which allows such an imported page to be treated
    /// as a group without further modification.
    PdfDictOptional group;

    /// A reference dictionary identifying a page to be imported from another
    /// PDF file, and for which the form XObject serves as a proxy (see 8.10.4,
    /// "Reference XObjects").
    PdfUnimplemented ref;

    /// A metadata stream containing metadata for the form XObject (see 14.3.2,
    /// "Metadata Streams").
    PdfUnimplemented metadata;

    /// A page-piece dictionary associated with the form XObject (see 14.5,
    /// "Page-Piece Dictionaries").
    PdfUnimplemented piece_info;

    /// The date and time (see 7.9.4, "Dates") when the form XObject’s contents
    /// were most recently modified. If a page-piece dictionary (PieceInfo) is
    /// present, the modification date shall be used to ascertain which of the
    /// application data dictionaries it contains correspond to the current
    /// content of the form (see 14.5, "Page-Piece Dictionaries").
    PdfUnimplemented last_modified;

    /// The integer key of the form XObject’s entry in the structural parent
    /// tree (see 14.7.4.4, "Finding Structure Elements from Content Items").
    PdfUnimplemented struct_parent;

    /// The integer key of the form XObject’s entry in the structural parent
    /// tree (see 14.7.4.4, "Finding Structure Elements from Content Items").
    PdfUnimplemented struct_parents;

    /// An OPI version dictionary for the form XObject (see 14.11.7, "Open
    /// Prepress Interface (OPI)").
    PdfUnimplemented opi;

    /// An optional content group or optional content membership dictionary
    /// (see 8.11, "Optional Content") specifying the optional content
    /// properties for the form XObject. Before the form is processed, its
    /// visibility shall be determined based on this entry. If it is determined
    /// to be invisible, the entire form shall be skipped, as if there were no
    /// Do operator to invoke it.
    PdfUnimplemented oc;

    /// The name by which this form XObject is referenced in the XObject
    /// subdictionary of the current resource dictionary (see 7.8.3, "Resource
    /// Dictionaries").
    PdfUnimplemented name;

    PdfContentStream content_stream;
} PdfFormXObject;

typedef struct {
    enum { PDF_XOBJECT_IMAGE, PDF_XOBJECT_FORM } type;
    union {
        PdfFormXObject form;
    } data;
} PdfXObject;

PdfError* pdf_deserialize_xobject(
    const PdfObject* object,
    PdfXObject* target_ptr,
    PdfOptionalResolver resolver,
    Arena* arena
);
