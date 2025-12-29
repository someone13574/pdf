#pragma once

#include "pdf/content_stream/operation.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"

typedef struct {
    PdfContentOpVec* operations;
} PdfContentStream;

PdfError* pdf_deserialize_content_stream(
    const PdfObject* object,
    PdfContentStream* deserialized,
    PdfResolver* resolver
);

DESERDE_DECL_RESOLVABLE(
    PdfContentStreamRef,
    PdfContentStream,
    pdf_content_stream_ref_init,
    pdf_resolve_content_stream
)

#define DVEC_NAME PdfContentStreamRefVec
#define DVEC_LOWERCASE_NAME pdf_content_stream_ref_vec
#define DVEC_TYPE PdfContentStreamRef
#include "arena/dvec_decl.h"

DESERDE_DECL_OPTIONAL(
    PdfContentsStreamRefVecOptional,
    PdfContentStreamRefVec*,
    pdf_content_stream_ref_vec_op_init
)
