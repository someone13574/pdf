#include "pdf/fonts/cmap.h"

#include "../deserialize.h"
#include "logger/log.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"

PdfError* pdf_deserialize_cid_system_info(
    const PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    PdfCIDSystemInfo* deserialized
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));
    RELEASE_ASSERT(deserialized);

    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            PdfCIDSystemInfo,
            "Registry",
            registry,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_STRING)
        ),
        PDF_FIELD(
            PdfCIDSystemInfo,
            "Ordering",
            ordering,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_STRING)
        ),
        PDF_FIELD(
            PdfCIDSystemInfo,
            "Supplement",
            supplement,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_STRING)
        )
    };

    deserialized->raw_dict = object;
    PDF_PROPAGATE(pdf_deserialize_object(
        deserialized,
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        arena,
        resolver,
        false
    ));

    return NULL;
}

PdfError* pdf_deserialize_cid_system_info_wrapper(
    const PdfObject* object,
    Arena* arena,
    PdfOptionalResolver resolver,
    void* deserialized
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(pdf_op_resolver_valid(resolver));
    RELEASE_ASSERT(deserialized);

    return pdf_deserialize_cid_system_info(
        object,
        arena,
        resolver,
        deserialized
    );
}

PdfError* pdf_parse_cmap(const uint8_t* data, size_t data_len) {
    RELEASE_ASSERT(data);
    (void)data_len;

    return NULL;
}
