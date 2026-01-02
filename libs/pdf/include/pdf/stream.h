#include "pdf/object.h"
#include "pdf/types.h"

struct PdfStreamDict {
    PdfInteger length;
    PdfNameVecOptional filter;

    const PdfObject* raw_dict;
};

Error* pdf_deserde_stream_dict(
    const PdfObject* object,
    PdfStreamDict* deserialized,
    PdfResolver* resolver
);
