#include "pdf/page.h"

#include <stdbool.h>
#include <string.h>

#include "arena/arena.h"
#include "err/error.h"
#include "logger/log.h"
#include "pdf/content_stream/stream.h"
#include "pdf/deserde.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf/resources.h"
#include "pdf/types.h"

PDF_IMPL_RESOLVABLE_FIELD(PdfPages, PdfPagesRef, pages)
PDF_IMPL_OPTIONAL_FIELD(PdfPagesRef, PdfPagesRefOptional, pages_ref)
PDF_IMPL_RESOLVABLE_FIELD(PdfPageTree, PdfPageTreeRef, page_tree)

#define DVEC_NAME PdfPageTreeRefVec
#define DVEC_LOWERCASE_NAME pdf_page_tree_ref_vec
#define DVEC_TYPE PdfPageTreeRef
#include "arena/dvec_impl.h"

PDF_IMPL_ARRAY_FIELD(PdfPageTreeRefVec, page_tree_ref_vec, page_tree_ref)

Error* pdf_deserde_page(
    const PdfObject* object,
    PdfPage* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    PdfFieldDescriptor fields[] = {
        pdf_name_field("Type", &target_ptr->type),
        pdf_pages_ref_field("Parent", &target_ptr->parent),
        pdf_ignored_field("LastModified", &target_ptr->last_modified),
        pdf_resources_optional_field("Resources", &target_ptr->resources),
        pdf_rectangle_optional_field("MediaBox", &target_ptr->media_box),
        pdf_rectangle_optional_field("CropBox", &target_ptr->crop_box),
        pdf_rectangle_optional_field("BleedBox", &target_ptr->bleed_box),
        pdf_rectangle_optional_field("TrimBox", &target_ptr->trim_box),
        pdf_rectangle_optional_field("ArtBox", &target_ptr->art_box),
        pdf_unimplemented_field("BoxColorInfo"),
        pdf_as_content_stream_ref_vec_optional_field(
            "Contents",
            &target_ptr->contents
        ),
        pdf_integer_optional_field("Rotate", &target_ptr->rotate),
        pdf_ignored_field("Group", &target_ptr->group), // TODO: group
        pdf_ignored_field("Thumb", &target_ptr->thumb),
        pdf_ignored_field("B", &target_ptr->b),
        pdf_ignored_field("Dur", &target_ptr->dur),
        pdf_ignored_field("Trans", &target_ptr->trans),
        pdf_ignored_field("Annots", &target_ptr->annots),
        pdf_ignored_field("AA", &target_ptr->aa),
        pdf_ignored_field("Metadata", &target_ptr->metadata),
        pdf_ignored_field("PieceInfo", &target_ptr->piece_info),
        pdf_unimplemented_field("StructParents"),
        pdf_ignored_field("ID", &target_ptr->id),
        pdf_ignored_field("PZ", &target_ptr->pz),
        pdf_unimplemented_field("SeparationInfo"),
        pdf_ignored_field("Tabs", &target_ptr->tabs),
        pdf_ignored_field(
            "TemplateInstantiated",
            &target_ptr->template_instantiated
        ),
        pdf_ignored_field("PresSteps", &target_ptr->pres_steps),
        pdf_unimplemented_field("UserUnit"),
        pdf_unimplemented_field("VP")
    };

    TRY(pdf_deserde_fields(
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

PDF_IMPL_RESOLVABLE_FIELD(PdfPage, PdfPageRef, page)

Error* pdf_deserde_pages(
    const PdfObject* object,
    PdfPages* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    PdfFieldDescriptor fields[] = {
        pdf_name_field("Type", &target_ptr->type),
        pdf_pages_ref_optional_field("Parent", &target_ptr->parent),
        pdf_page_tree_ref_vec_field("Kids", &target_ptr->kids),
        pdf_integer_field("Count", &target_ptr->count),
        pdf_resources_optional_field("Resources", &target_ptr->resources),
        pdf_rectangle_optional_field("MediaBox", &target_ptr->media_box),
        pdf_rectangle_optional_field("CropBox", &target_ptr->crop_box),
        pdf_integer_optional_field("Rotate", &target_ptr->rotate)
    };

    TRY(pdf_deserde_fields(
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

Error* pdf_deserde_page_tree(
    const PdfObject* object,
    PdfPageTree* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    PdfName type = NULL;
    PdfFieldDescriptor stub_fields[] = {pdf_name_field("Type", &type)};

    TRY(pdf_deserde_fields(
        object,
        stub_fields,
        1,
        true,
        resolver,
        "PageTree type stub"
    ));

    if (strcmp(type, "Page") == 0) {
        target_ptr->kind = PDF_PAGE_TREE_PAGE;
        TRY(pdf_deserde_page(object, &target_ptr->value.page, resolver));
    } else if (strcmp(type, "Page") == 0) {
        target_ptr->kind = PDF_PAGE_TREE_PAGES;
        TRY(pdf_deserde_pages(object, &target_ptr->value.pages, resolver));
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
        if (src->resources.is_some && !dst->value.page.resources.is_some) {
            dst->value.page.resources = src->resources;
        }

        if (src->media_box.is_some && !dst->value.page.media_box.is_some) {
            dst->value.page.media_box = src->media_box;
        }

        if (src->crop_box.is_some && !dst->value.page.crop_box.is_some) {
            dst->value.page.crop_box = src->crop_box;
        }

        if (src->rotate.is_some && !dst->value.page.rotate.is_some) {
            dst->value.page.rotate = src->rotate;
        }
    } else {
        if (src->resources.is_some && !dst->value.pages.resources.is_some) {
            dst->value.pages.resources = src->resources;
        }

        if (src->media_box.is_some && !dst->value.pages.media_box.is_some) {
            dst->value.pages.media_box = src->media_box;
        }

        if (src->crop_box.is_some && !dst->value.pages.crop_box.is_some) {
            dst->value.pages.crop_box = src->crop_box;
        }

        if (src->rotate.is_some && !dst->value.pages.rotate.is_some) {
            dst->value.pages.rotate = src->rotate;
        }
    }
}

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

    TRY(pdf_resolve_pages(&root_ref, resolver));
    pdf_page_iter_frame_vec_push(
        iter->stack,
        (PdfPageIterFrame) {.node = *root_ref.resolved, .next_child_idx = 0}
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

        TRY(pdf_resolve_page_tree(&kid_ref, iter->resolver));
        pdf_page_tree_inherit(kid_ref.resolved, &curr_frame->node);

        if (kid_ref.resolved->kind == PDF_PAGE_TREE_PAGE) {
            if (out_page) {
                *out_page = kid_ref.resolved->value.page;
            }
            *done = false;
            iter->page_idx += 1;
            return NULL;
        } else {
            pdf_page_iter_frame_vec_push(
                iter->stack,
                (PdfPageIterFrame) {.node = kid_ref.resolved->value.pages,
                                    .next_child_idx = 0}
            );
            RELEASE_ASSERT(pdf_page_iter_frame_vec_len(iter->stack) <= 1024);
        }
    }

    return NULL;
}
