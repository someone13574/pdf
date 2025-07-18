#include "deserialize.h"
#include "log.h"
#include "pdf_catalog.h"
#include "pdf_doc.h"
#include "pdf_object.h"
#include "pdf_result.h"
#include "pdf_trailer.h"

PdfResult pdf_deserialize_trailer(
    PdfObject* object,
    PdfDocument* doc,
    PdfTrailer* deserialized
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(doc);
    RELEASE_ASSERT(deserialized);

    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            PdfTrailer,
            "Size",
            size,
            PDF_OBJECT_FIELD(PDF_OBJECT_TYPE_INTEGER)
        ),
        PDF_FIELD(PdfTrailer, "Root", root, PDF_REF_FIELD(PdfCatalogRef))
    };

    deserialized->raw_dict = object;
    PDF_PROPAGATE(pdf_deserialize_object(
        deserialized,
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        doc
    ));

    return PDF_OK;
}
