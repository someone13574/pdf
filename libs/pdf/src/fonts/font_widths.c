#include "pdf/fonts/font_widths.h"

#include <stdbool.h>

#include "../deserialize.h"
#include "logger/log.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"

#define DVEC_NAME PdfFontWidthVec
#define DVEC_LOWERCASE_NAME pdf_font_width_vec
#define DVEC_TYPE PdfFontWidthEntry
#include "arena/dvec_impl.h"

PdfError* pdf_deserialize_font_widths(
    const PdfObject* object,
    PdfFontWidths* deserialized,
    PdfOptionalResolver resolver,
    Arena* arena
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(deserialized);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));
    RELEASE_ASSERT(arena);

    if (!deserialized->cid_to_width) {
        deserialized->cid_to_width = pdf_font_width_vec_new(arena);
    }

    deserialized->raw_dict = object;

    PdfObject resolved_object;
    pdf_resolve_object(resolver, object, &resolved_object);

    if (resolved_object.type != PDF_OBJECT_TYPE_ARRAY) {
        return PDF_ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "Font width array must be an array"
        );
    }

    // When we've read two integers in a row, we know it is a `c_first c_last w`
    // range.
    size_t integers_read = 0;
    PdfInteger start_cid;
    PdfInteger end_cid;

    // Track the length of the lookup table so we can add entries as required
    size_t lookup_len = pdf_font_width_vec_len(deserialized->cid_to_width);

    for (size_t idx = 0;
         idx < pdf_object_vec_len(resolved_object.data.array.elements);
         idx++) {
        PdfObject* element;
        RELEASE_ASSERT(pdf_object_vec_get(
            resolved_object.data.array.elements,
            idx,
            &element
        ));

        if (element->type == PDF_OBJECT_TYPE_INTEGER) {
            switch (integers_read) {
                case 0: {
                    start_cid = element->data.integer;
                    integers_read = 1;
                    break;
                }
                case 1: {
                    end_cid = element->data.integer;
                    integers_read = 2;
                    break;
                }
                case 2: {
                    for (size_t cid = (size_t)start_cid; cid <= (size_t)end_cid;
                         cid++) {
                        // Grow lookup table
                        while (cid >= lookup_len) {
                            pdf_font_width_vec_push(
                                deserialized->cid_to_width,
                                (PdfFontWidthEntry) {.has_value = false}
                            );
                            lookup_len++;
                        }

                        // Set entry
                        PdfFontWidthEntry* entry = NULL;
                        RELEASE_ASSERT(pdf_font_width_vec_get_ptr(
                            deserialized->cid_to_width,
                            cid,
                            &entry
                        ));

                        entry->has_value = true;
                        entry->width = element->data.integer;
                    }

                    integers_read = 0;
                    break;
                }
                default: {
                    LOG_PANIC("Unreachable");
                }
            }
        } else if (element->type == PDF_OBJECT_TYPE_ARRAY) {
            if (integers_read != 1) {
                return PDF_ERROR(
                    PDF_ERR_INCORRECT_TYPE,
                    "Array of widths must have exactly one preceding integer"
                );
            }

            size_t num_widths =
                pdf_object_vec_len(element->data.array.elements);
            if (num_widths == 0) {
                LOG_WARN(FONT, "Empty array of widths");
                continue;
            }

            end_cid = start_cid + (PdfInteger)num_widths - 1;

            // Grow lookup table
            while ((size_t)end_cid >= lookup_len) {
                pdf_font_width_vec_push(
                    deserialized->cid_to_width,
                    (PdfFontWidthEntry) {.has_value = false}
                );
                lookup_len++;
            }

            // Set entries
            for (size_t cid = (size_t)start_cid; cid <= (size_t)end_cid;
                 cid++) {
                PdfFontWidthEntry* entry = NULL;
                RELEASE_ASSERT(pdf_font_width_vec_get_ptr(
                    deserialized->cid_to_width,
                    cid,
                    &entry
                ));

                PdfObject* subarray_element;
                RELEASE_ASSERT(pdf_object_vec_get(
                    element->data.array.elements,
                    cid - (size_t)start_cid,
                    &subarray_element
                ));

                if (subarray_element->type != PDF_OBJECT_TYPE_INTEGER) {
                    return PDF_ERROR(
                        PDF_ERR_INCORRECT_TYPE,
                        "Object type in array of widths must be an integer"
                    );
                }

                entry->has_value = true;
                entry->width = subarray_element->data.integer;
            }

            integers_read = 0;
        } else {
            return PDF_ERROR(
                PDF_ERR_INCORRECT_TYPE,
                "Only integers and arrays of integers can be in a widths array"
            );
        }
    }

    return NULL;
}

DESERDE_IMPL_TRAMPOLINE(
    pdf_deserialize_font_widths_trampoline,
    pdf_deserialize_font_widths
)
DESERDE_IMPL_OPTIONAL(PdfFontWidthsOptional, pdf_font_widths_op_init)
