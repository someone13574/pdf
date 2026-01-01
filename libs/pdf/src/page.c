#include "pdf/page.h"

#include <stdbool.h>
#include <string.h>

#include "arena/arena.h"
#include "deser.h"
#include "err/error.h"
#include "logger/log.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf/resources.h"

#define DVEC_NAME PdfPageTreeRefVec
#define DVEC_LOWERCASE_NAME pdf_page_tree_ref_vec
#define DVEC_TYPE PdfPageTreeRef
#include "arena/dvec_impl.h"

Error* pdf_deser_page(
    const PdfObject* object,
    PdfPage* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            "Type",
            &target_ptr->type,
            PDF_DESER_OBJECT(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(
            "Parent",
            &target_ptr->parent,
            PDF_DESER_RESOLVABLE(pdf_pages_ref_init)
        ),
        PDF_IGNORED_FIELD("LastModified", &target_ptr->last_modified),
        PDF_FIELD(
            "Resources",
            &target_ptr->resources,
            PDF_DESER_OPTIONAL(
                pdf_resources_op_init,
                PDF_DESER_CUSTOM(pdf_deser_resources_trampoline)
            )
        ),
        PDF_FIELD(
            "MediaBox",
            &target_ptr->media_box,
            PDF_DESER_OPTIONAL(
                pdf_rectangle_op_init,
                PDF_DESER_CUSTOM(pdf_deser_rectangle_trampoline)
            )
        ),
        PDF_FIELD(
            "CropBox",
            &target_ptr->crop_box,
            PDF_DESER_OPTIONAL(
                pdf_rectangle_op_init,
                PDF_DESER_CUSTOM(pdf_deser_rectangle_trampoline)
            )
        ),
        PDF_FIELD(
            "BleedBox",
            &target_ptr->bleed_box,
            PDF_DESER_OPTIONAL(
                pdf_rectangle_op_init,
                PDF_DESER_CUSTOM(pdf_deser_rectangle_trampoline)
            )
        ),
        PDF_FIELD(
            "TrimBox",
            &target_ptr->trim_box,
            PDF_DESER_OPTIONAL(
                pdf_rectangle_op_init,
                PDF_DESER_CUSTOM(pdf_deser_rectangle_trampoline)
            )
        ),
        PDF_FIELD(
            "ArtBox",
            &target_ptr->art_box,
            PDF_DESER_OPTIONAL(
                pdf_rectangle_op_init,
                PDF_DESER_CUSTOM(pdf_deser_rectangle_trampoline)
            )
        ),
        PDF_UNIMPLEMENTED_FIELD("BoxColorInfo"),
        PDF_FIELD(
            "Contents",
            &target_ptr->contents,
            PDF_DESER_OPTIONAL(
                pdf_content_stream_ref_vec_op_init,
                PDF_DESER_AS_ARRAY(
                    pdf_content_stream_ref_vec_push_uninit,
                    PDF_DESER_RESOLVABLE(pdf_content_stream_ref_init)
                )
            )
        ),
        PDF_FIELD(
            "Rotate",
            &target_ptr->rotate,
            PDF_DESER_OPTIONAL(
                pdf_integer_op_init,
                PDF_DESER_OBJECT(PDF_OBJECT_TYPE_INTEGER)
            )
        ),
        PDF_IGNORED_FIELD("Group", &target_ptr->group), // TODO: group
        PDF_IGNORED_FIELD("Thumb", &target_ptr->thumb),
        PDF_IGNORED_FIELD("B", &target_ptr->b),
        PDF_IGNORED_FIELD("Dur", &target_ptr->dur),
        PDF_IGNORED_FIELD("Trans", &target_ptr->trans),
        PDF_IGNORED_FIELD("Annots", &target_ptr->annots),
        PDF_IGNORED_FIELD("AA", &target_ptr->aa),
        PDF_IGNORED_FIELD("Metadata", &target_ptr->metadata),
        PDF_IGNORED_FIELD("PieceInfo", &target_ptr->piece_info),
        PDF_UNIMPLEMENTED_FIELD("StructParents"),
        PDF_IGNORED_FIELD("ID", &target_ptr->id),
        PDF_IGNORED_FIELD("PZ", &target_ptr->pz),
        PDF_UNIMPLEMENTED_FIELD("SeparationInfo"),
        PDF_IGNORED_FIELD("Tabs", &target_ptr->tabs),
        PDF_IGNORED_FIELD(
            "TemplateInstantiated",
            &target_ptr->template_instantiated
        ),
        PDF_IGNORED_FIELD("PresSteps", &target_ptr->pres_steps),
        PDF_UNIMPLEMENTED_FIELD("UserUnit"),
        PDF_UNIMPLEMENTED_FIELD("VP")
    };

    TRY(pdf_deser_dict(
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        false,
        resolver,
        "Page"
    ));

    if (strcmp(target_ptr->type, "Page") != 0) {
        return ERROR(PDF_ERR_INVALID_SUBTYPE, "`Type` must be `Page`");
    }

    return NULL;
}

DESER_IMPL_RESOLVABLE(
    PdfPageRef,
    PdfPage,
    pdf_page_ref_init,
    pdf_resolve_page,
    pdf_deser_page
)

Error* pdf_deser_pages(
    const PdfObject* object,
    PdfPages* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            "Type",
            &target_ptr->type,
            PDF_DESER_OBJECT(PDF_OBJECT_TYPE_NAME)
        ),
        PDF_FIELD(
            "Parent",
            &target_ptr->parent,
            PDF_DESER_OPTIONAL(
                pdf_pages_ref_op_init,
                PDF_DESER_RESOLVABLE(pdf_pages_ref_init)
            )
        ),
        PDF_FIELD(
            "Kids",
            &target_ptr->kids,
            PDF_DESER_ARRAY(
                pdf_page_tree_ref_vec_push_uninit,
                PDF_DESER_RESOLVABLE(pdf_page_tree_ref_init)
            )
        ),
        PDF_FIELD(
            "Count",
            &target_ptr->count,
            PDF_DESER_OBJECT(PDF_OBJECT_TYPE_INTEGER)
        ),
        PDF_FIELD(
            "Resources",
            &target_ptr->resources,
            PDF_DESER_OPTIONAL(
                pdf_resources_op_init,
                PDF_DESER_CUSTOM(pdf_deser_resources_trampoline)
            )
        ),
        PDF_FIELD(
            "MediaBox",
            &target_ptr->media_box,
            PDF_DESER_OPTIONAL(
                pdf_rectangle_op_init,
                PDF_DESER_CUSTOM(pdf_deser_rectangle_trampoline)
            )
        ),
        PDF_FIELD(
            "CropBox",
            &target_ptr->crop_box,
            PDF_DESER_OPTIONAL(
                pdf_rectangle_op_init,
                PDF_DESER_CUSTOM(pdf_deser_rectangle_trampoline)
            )
        ),
        PDF_FIELD(
            "Rotate",
            &target_ptr->rotate,
            PDF_DESER_OPTIONAL(
                pdf_integer_op_init,
                PDF_DESER_OBJECT(PDF_OBJECT_TYPE_INTEGER)
            )
        ),
    };

    TRY(pdf_deser_dict(
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        false,
        resolver,
        "Pages"
    ));

    if (strcmp(target_ptr->type, "Pages") != 0) {
        return ERROR(PDF_ERR_INVALID_SUBTYPE, "`Type` must be `Pages`");
    }

    return NULL;
}

Error* pdf_deser_page_tree(
    const PdfObject* object,
    PdfPageTree* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    PdfName type = NULL;
    PdfFieldDescriptor stub_fields[] = {
        PDF_FIELD("Type", &type, PDF_DESER_OBJECT(PDF_OBJECT_TYPE_NAME))
    };

    TRY(pdf_deser_dict(
        object,
        stub_fields,
        1,
        true,
        resolver,
        "PageTree type stub"
    ));

    if (strcmp(type, "Page") == 0) {
        target_ptr->kind = PDF_PAGE_TREE_PAGE;
        TRY(pdf_deser_page(object, &target_ptr->value.page, resolver));
    } else if (strcmp(type, "Page") == 0) {
        target_ptr->kind = PDF_PAGE_TREE_PAGES;
        TRY(pdf_deser_pages(object, &target_ptr->value.pages, resolver));
    } else {
        return ERROR(
            PDF_ERR_INVALID_SUBTYPE,
            "`Type` must be `Page` or `Pages`"
        );
    }

    return NULL;
}

void pdf_page_tree_inherit(PdfPageTree* dst, PdfPages* src) {
    RELEASE_ASSERT(dst);
    RELEASE_ASSERT(src);

    if (dst->kind == PDF_PAGE_TREE_PAGE) {
        if (src->resources.has_value && !dst->value.page.resources.has_value) {
            dst->value.page.resources = src->resources;
        }

        if (src->media_box.has_value && !dst->value.page.media_box.has_value) {
            dst->value.page.media_box = src->media_box;
        }

        if (src->crop_box.has_value && !dst->value.page.crop_box.has_value) {
            dst->value.page.crop_box = src->crop_box;
        }

        if (src->rotate.has_value && !dst->value.page.rotate.has_value) {
            dst->value.page.rotate = src->rotate;
        }
    } else {
        if (src->resources.has_value && !dst->value.pages.resources.has_value) {
            dst->value.pages.resources = src->resources;
        }

        if (src->media_box.has_value && !dst->value.pages.media_box.has_value) {
            dst->value.pages.media_box = src->media_box;
        }

        if (src->crop_box.has_value && !dst->value.pages.crop_box.has_value) {
            dst->value.pages.crop_box = src->crop_box;
        }

        if (src->rotate.has_value && !dst->value.pages.rotate.has_value) {
            dst->value.pages.rotate = src->rotate;
        }
    }
}

DESER_IMPL_RESOLVABLE(
    PdfPagesRef,
    PdfPages,
    pdf_pages_ref_init,
    pdf_resolve_pages,
    pdf_deser_pages
)

DESER_IMPL_OPTIONAL(PdfPagesRefOptional, pdf_pages_ref_op_init)

DESER_IMPL_RESOLVABLE(
    PdfPageTreeRef,
    PdfPageTree,
    pdf_page_tree_ref_init,
    pdf_resolve_page_tree,
    pdf_deser_page_tree
)

typedef struct {
    PdfPages node;
    size_t next_child_idx;
} PdfPageIterFrame;

#define DVEC_NAME PdfPageIterFrameVec
#define DVEC_LOWERCASE_NAME pdf_page_iter_frame_vec
#define DVEC_TYPE PdfPageIterFrame
#include "arena/dvec_impl.h"

struct PdfPageIter {
    PdfResolver* resolver;
    PdfPageIterFrameVec* stack;
    size_t page_idx;
};

Error* pdf_page_iter_new(
    PdfResolver* resolver,
    PdfPagesRef root_ref,
    PdfPageIter** iter_out
) {
    RELEASE_ASSERT(resolver);
    RELEASE_ASSERT(iter_out);

    PdfPageIter* iter =
        arena_alloc(pdf_resolver_arena(resolver), sizeof(PdfPageIter));

    iter->resolver = resolver;
    iter->stack = pdf_page_iter_frame_vec_new(pdf_resolver_arena(resolver));
    iter->page_idx = 0;

    PdfPages root;
    TRY(pdf_resolve_pages(root_ref, resolver, &root));
    pdf_page_iter_frame_vec_push(
        iter->stack,
        (PdfPageIterFrame) {.node = root, .next_child_idx = 0}
    );

    *iter_out = iter;
    return NULL;
}

Error* pdf_page_iter_next(PdfPageIter* iter, PdfPage* out_page, bool* done) {
    RELEASE_ASSERT(iter);
    RELEASE_ASSERT(done);

    *done = true;

    while (pdf_page_iter_frame_vec_len(iter->stack) != 0) {
        size_t depth = pdf_page_iter_frame_vec_len(iter->stack);
        RELEASE_ASSERT(depth > 0);

        PdfPageIterFrame* curr_frame = NULL;
        RELEASE_ASSERT(
            pdf_page_iter_frame_vec_get_ptr(iter->stack, depth - 1, &curr_frame)
        );

        PdfPageTreeRef kid_ref;
        if (!pdf_page_tree_ref_vec_get(
                curr_frame->node.kids,
                curr_frame->next_child_idx,
                &kid_ref
            )) {
            RELEASE_ASSERT(pdf_page_iter_frame_vec_pop(iter->stack, NULL));
            continue;
        }
        curr_frame->next_child_idx += 1;

        PdfPageTree kid;
        TRY(pdf_resolve_page_tree(kid_ref, iter->resolver, &kid));
        pdf_page_tree_inherit(&kid, &curr_frame->node);

        if (kid.kind == PDF_PAGE_TREE_PAGE) {
            if (out_page) {
                *out_page = kid.value.page;
            }
            *done = false;
            iter->page_idx += 1;
            return NULL;
        } else {
            pdf_page_iter_frame_vec_push(
                iter->stack,
                (PdfPageIterFrame) {.node = kid.value.pages,
                                    .next_child_idx = 0}
            );
            RELEASE_ASSERT(pdf_page_iter_frame_vec_len(iter->stack) <= 1024);
        }
    }

    return NULL;
}
