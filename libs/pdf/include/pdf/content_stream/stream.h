#pragma once

#include "err/error.h"
#include "pdf/content_stream/operation.h"
#include "pdf/object.h"
#include "pdf/resolver.h"

typedef struct {
    PdfContentOpVec* operations;
} PdfContentStream;

Error* pdf_deser_content_stream(
    const PdfObject* object,
    PdfContentStream* deserialized,
    PdfResolver* resolver
);

DESER_DECL_RESOLVABLE(
    PdfContentStreamRef,
    PdfContentStream,
    pdf_content_stream_ref_init,
    pdf_resolve_content_stream
)

#define DVEC_NAME PdfContentStreamRefVec
#define DVEC_LOWERCASE_NAME pdf_content_stream_ref_vec
#define DVEC_TYPE PdfContentStreamRef
#include "arena/dvec_decl.h"

DESER_DECL_OPTIONAL(
    PdfContentsStreamRefVecOptional,
    PdfContentStreamRefVec*,
    pdf_content_stream_ref_vec_op_init
)
