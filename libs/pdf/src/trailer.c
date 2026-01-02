#include "pdf/trailer.h"

#include "err/error.h"
#include "logger/log.h"
#include "pdf/catalog.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf/types.h"

Error* pdf_deserde_trailer(
    const PdfObject* object,
    PdfTrailer* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(resolver);
    RELEASE_ASSERT(target_ptr);

    PdfFieldDescriptor fields[] = {
        pdf_integer_field("Size", &target_ptr->size),
        pdf_integer_optional_field("Prev", &target_ptr->prev),
        pdf_catalog_ref_field("Root", &target_ptr->root),
        pdf_dict_optional_field("Info", &target_ptr->info),
        pdf_array_optional_field("ID", &target_ptr->id)
    };

    TRY(pdf_deserde_fields(
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        false,
        resolver,
        "PdfTrailer"
    ));

    return NULL;
}
