#pragma once

#include "err/error.h"
#include "pdf/content_stream/operation.h"
#include "pdf/deserde.h"
#include "pdf/object.h"
#include "pdf/resolver.h"

typedef struct {
    PdfContentOpVec* operations;
} PdfContentStream;

Error* pdf_deserde_content_stream(
    const PdfObject* object,
    PdfContentStream* deserialized,
    PdfResolver* resolver
);

PDF_DECL_RESOLVABLE_FIELD(PdfContentStream, PdfContentStreamRef, content_stream)

#define DVEC_NAME PdfContentStreamRefVec
#define DVEC_LOWERCASE_NAME pdf_content_stream_ref_vec
#define DVEC_TYPE PdfContentStreamRef
#include "arena/dvec_decl.h"

PDF_DECL_OPTIONAL_FIELD(
    PdfContentStreamRefVec*,
    PdfContentStreamRefVecOptional,
    content_stream_ref_vec
)
