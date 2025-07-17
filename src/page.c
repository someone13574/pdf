#include "deserialize.h"
#include "pdf_doc.h"
#include "pdf_object.h"
#include "pdf_page.h"

PDF_DESERIALIZABLE_REF_IMPL(
    PdfPageTreeNode,
    page_tree_node,
    pdf_deserialize_page_tree_node
)

PdfPageTreeNode* pdf_deserialize_page_tree_node(
    PdfObject* object,
    PdfDocument* doc,
    PdfResult* result
) {
    if (!object || !doc || !result) {
        return NULL;
    }

    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            PdfPageTreeNode,
            "Type",
            type,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(
            PdfPageTreeNode,
            "Parent",
            parent,
            PDF_OPTIONAL_FIELD(
                PdfOpDict,
                PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_DICT)
            )
        ),
        PDF_FIELD(
            PdfPageTreeNode,
            "Kids",
            kids,
            PDF_ARRAY_FIELD(PdfPageArray, PdfPageRef, PDF_REF_FIELD(PdfPageRef))
        ),
        PDF_FIELD(
            PdfPageTreeNode,
            "Count",
            count,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_INTEGER)
        )
    };

    PdfPageTreeNode page_tree_node = {.raw_dict = object};
    *result = pdf_deserialize_object(
        &page_tree_node,
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        doc
    );

    if (*result != PDF_OK) {
        return NULL;
    }

    PdfPageTreeNode* allocated =
        arena_alloc(pdf_doc_arena(doc), sizeof(PdfPageTreeNode));
    *allocated = page_tree_node;

    return allocated;
}

PdfPage*
pdf_deserialize_page(PdfObject* object, PdfDocument* doc, PdfResult* result) {
    if (!object || !doc || !result) {
        return NULL;
    }

    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            PdfPage,
            "Type",
            type,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(PdfPage, "Parent", parent, PDF_REF_FIELD(PdfPageTreeNodeRef)),
        PDF_FIELD(
            PdfPage,
            "Resources",
            resources,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_DICT)
        ),
        PDF_FIELD(
            PdfPage,
            "MediaBox",
            media_box,
            PDF_OPTIONAL_FIELD(
                PdfOpArray,
                PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_ARRAY)
            )
        ),
        PDF_FIELD(
            PdfPage,
            "Contents",
            contents,
            PDF_OPTIONAL_FIELD(
                PdfOpContentsArray,
                PDF_AS_ARRAY_FIELD(
                    PdfContentsArray,
                    PdfStream,
                    PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_STREAM)
                )
            )
        )
    };

    PdfPage page = {.raw_dict = object};
    *result = pdf_deserialize_object(
        &page,
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        doc
    );

    if (*result != PDF_OK) {
        return NULL;
    }

    PdfPage* allocated = arena_alloc(pdf_doc_arena(doc), sizeof(PdfPage));
    *allocated = page;

    return allocated;
}

PDF_DESERIALIZABLE_REF_IMPL(PdfPage, page, pdf_deserialize_page)
