#include "deserialize.h"
#include "log.h"
#include "pdf_object.h"
#include "pdf_page.h"
#include "pdf_resolver.h"
#include "pdf_result.h"

PDF_DESERIALIZABLE_REF_IMPL(
    PdfPageTreeNode,
    page_tree_node,
    pdf_deserialize_page_tree_node
)

PdfResult pdf_deserialize_page_tree_node(
    PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    PdfPageTreeNode* deserialized
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));
    RELEASE_ASSERT(deserialized);

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

    deserialized->raw_dict = object;
    PDF_PROPAGATE(pdf_deserialize_object(
        deserialized,
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        arena,
        resolver
    ));

    return PDF_OK;
}

PdfResult pdf_deserialize_page(
    PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    PdfPage* deserialized
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));
    RELEASE_ASSERT(deserialized);

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

    deserialized->raw_dict = object;
    PDF_PROPAGATE(pdf_deserialize_object(
        deserialized,
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        arena,
        resolver
    ));

    return PDF_OK;
}

PDF_DESERIALIZABLE_REF_IMPL(PdfPage, page, pdf_deserialize_page)
