#pragma once

#include "pdf/deserde.h"
#include "pdf/object.h"
#include "pdf/resolver.h"

typedef struct {
    enum { PDF_CID_TO_GID_MAP_IDENTITY, PDF_CID_TO_GID_MAP_STREAM } type;
    union {
        PdfName name;
        PdfStream stream;
    } value;
} PdfCIDToGIDMap;

Error* pdf_deserde_cid_to_gid_map(
    const PdfObject* object,
    PdfCIDToGIDMap* target_ptr,
    PdfResolver* resolver
);

PDF_DECL_OPTIONAL_FIELD(PdfCIDToGIDMap, PdfCIDToGIDMapOptional, cid_to_gid_map)

Error* pdf_map_cid_to_gid(PdfCIDToGIDMap* map, uint32_t cid, uint32_t* gid_out);
