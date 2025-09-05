#include "pdf/page.h"

#include "deserialize.h"
#include "logger/log.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf/resources/resources.h"
#include "pdf_error/error.h"

PDF_DESERIALIZABLE_REF_IMPL(
    PdfPageTreeNode,
    page_tree_node,
    pdf_deserialize_page_tree_node
)

PdfError* pdf_deserialize_page_tree_node(
    const PdfObject* object,
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

    return NULL;
}

PdfError* pdf_deserialize_page(
    const PdfObject* object,
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
            PDF_OPTIONAL_FIELD(
                PdfOpResources,
                PDF_CUSTOM_FIELD(pdf_deserialize_resources_wrapper)
            )
        ),
        PDF_FIELD(
            PdfPage,
            "MediaBox",
            media_box,
            PDF_CUSTOM_FIELD(pdf_deserialize_rectangle_wrapper)
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

    return NULL;
}

PDF_DESERIALIZABLE_REF_IMPL(PdfPage, page, pdf_deserialize_page)
