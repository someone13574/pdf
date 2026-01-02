#include "pdf/fonts/cid_to_gid_map.h"

#include <string.h>

#include "err/error.h"
#include "logger/log.h"
#include "pdf/object.h"
#include "pdf/resolver.h"

Error* pdf_deserde_cid_to_gid_map(
    const PdfObject* object,
    PdfCIDToGIDMap* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    PdfObject resolved;
    TRY(pdf_resolve_object(resolver, object, &resolved, true));

    if (resolved.type == PDF_OBJECT_TYPE_NAME) {
        if (strcmp(resolved.data.name, "Identity") != 0) {
            return ERROR(
                PDF_ERR_INCORRECT_TYPE,
                "CIDToGIDMap name must be identity"
            );
        }

        target_ptr->type = PDF_CID_TO_GID_MAP_IDENTITY;
        target_ptr->value.name = resolved.data.name;
    } else {
        target_ptr->type = PDF_CID_TO_GID_MAP_STREAM;
        target_ptr->value.stream = resolved.data.stream;
    }

    return NULL;
}

Error*
pdf_map_cid_to_gid(PdfCIDToGIDMap* map, uint32_t cid, uint32_t* gid_out) {
    RELEASE_ASSERT(map);
    RELEASE_ASSERT(gid_out);

    if (map->type == PDF_CID_TO_GID_MAP_IDENTITY) {
        *gid_out = cid;
    } else {
        if (cid * 2 + 1 >= map->value.stream.decoded_stream_len) {
            return ERROR(
                PDF_ERR_INVALID_CID,
                "Cid `%zu` out-of-bounds in cid-to-gid map",
                (size_t)cid
            );
        }

        uint32_t b0 = map->value.stream.stream_bytes[(size_t)cid * 2];
        uint32_t b1 = map->value.stream.stream_bytes[(size_t)cid * 2 + 1];
        *gid_out = (b0 << 8) | b1;
    }

    return NULL;
}
