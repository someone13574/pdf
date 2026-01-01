#pragma once

#include "pdf/object.h"
#include "pdf/resolver.h"

typedef struct {
    enum { PDF_CID_TO_GID_MAP_IDENTITY, PDF_CID_TO_GID_MAP_STREAM } type;
    union {
        PdfName name;
        PdfStream stream;
    } value;
} PdfCIDToGIDMap;

Error* pdf_deserialize_cid_to_gid_map(
    const PdfObject* object,
    PdfCIDToGIDMap* target_ptr,
    PdfResolver* resolver
);

Error* pdf_map_cid_to_gid(PdfCIDToGIDMap* map, uint32_t cid, uint32_t* gid_out);

DESERDE_DECL_OPTIONAL(
    PdfCIDToGIDMapOptional,
    PdfCIDToGIDMap,
    pdf_cid_to_gid_map_op_init
)

DESERDE_DECL_TRAMPOLINE(pdf_deserialize_cid_to_gid_map_trampoline)
