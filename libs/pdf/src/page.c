#include "pdf/page.h"

#include "deserialize.h"
#include "logger/log.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf/resources.h"
#include "pdf_error/error.h"

#define DVEC_NAME PdfPageRefVec
#define DVEC_LOWERCASE_NAME pdf_page_ref_vec
#define DVEC_TYPE PdfPageRef
#include "arena/dvec_impl.h"

PdfError* pdf_deserialize_page(
    const PdfObject* object,
    PdfPage* target_ptr,
    PdfOptionalResolver resolver,
    Arena* arena
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));
    RELEASE_ASSERT(arena);

    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            "Type",
            &target_ptr->type,
            PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(
            "Parent",
            &target_ptr->parent,
            PDF_DESERDE_RESOLVABLE(pdf_page_tree_node_ref_init)
        ),
        PDF_FIELD(
            "Resources",
            &target_ptr->resources,
            PDF_DESERDE_OPTIONAL(
                pdf_resources_op_init,
                PDF_DESERDE_CUSTOM(pdf_deserialize_resources_trampoline)
            )
        ),
        PDF_FIELD(
            "MediaBox",
            &target_ptr->media_box,
            PDF_DESERDE_CUSTOM(pdf_deserialize_rectangle_trampoline)
        ),
        PDF_FIELD(
            "Contents",
            &target_ptr->contents,
            PDF_DESERDE_OPTIONAL(
                pdf_content_stream_ref_vec_op_init,
                PDF_DESERDE_AS_ARRAY(
                    pdf_content_stream_ref_vec_push_uninit,
                    PDF_DESERDE_RESOLVABLE(pdf_content_stream_ref_init)
                )
            )
        ),
        PDF_FIELD(
            "Rotate",
            &target_ptr->rotate,
            PDF_DESERDE_OPTIONAL(
                pdf_integer_op_init,
                PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_INTEGER)
            )
        ),
        PDF_FIELD(
            "Group",
            &target_ptr->group,
            PDF_DESERDE_OPTIONAL(
                pdf_dict_op_init,
                PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_DICT)
            )
        )
    };

    PDF_PROPAGATE(pdf_deserialize_dict(
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        false,
        resolver,
        arena,
        "PdfPage"
    ));

    return NULL;
}

DESERDE_IMPL_RESOLVABLE(
    PdfPageRef,
    PdfPage,
    pdf_page_ref_init,
    pdf_resolve_page,
    pdf_deserialize_page
)

PdfError* pdf_deserialize_page_tree_node(
    const PdfObject* object,
    PdfPageTreeNode* target_ptr,
    PdfOptionalResolver resolver,
    Arena* arena
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));
    RELEASE_ASSERT(arena);

    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            "Type",
            &target_ptr->type,
            PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(
            "Parent",
            &target_ptr->parent,
            PDF_DESERDE_OPTIONAL(
                pdf_dict_op_init,
                PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_DICT)
            )
        ),
        PDF_FIELD(
            "Kids",
            &target_ptr->kids,
            PDF_DESERDE_ARRAY(
                pdf_page_ref_vec_push_uninit,
                PDF_DESERDE_RESOLVABLE(pdf_page_ref_init)
            )
        ),
        PDF_FIELD(
            "Count",
            &target_ptr->count,
            PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_INTEGER)
        )
    };

    PDF_PROPAGATE(pdf_deserialize_dict(
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        false,
        resolver,
        arena,
        "PdfPageTreeNode"
    ));

    return NULL;
}

DESERDE_IMPL_RESOLVABLE(
    PdfPageTreeNodeRef,
    PdfPageTreeNode,
    pdf_page_tree_node_ref_init,
    pdf_resolve_page_tree_node,
    pdf_deserialize_page_tree_node
)
