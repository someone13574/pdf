#include "header.h"

#include "err/error.h"
#include "logger/log.h"
#include "parse_ctx/binary.h"
#include "parse_ctx/ctx.h"
#include "types.h"

Error* cff_read_header(ParseCtx* ctx, CffHeader* cff_header_out) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(cff_header_out);

    TRY(parse_ctx_seek(ctx, 0));
    TRY(parse_ctx_read_u8(ctx, &cff_header_out->major));
    TRY(parse_ctx_read_u8(ctx, &cff_header_out->minor));
    TRY(parse_ctx_read_u8(ctx, &cff_header_out->header_size));
    TRY(cff_read_offset_size(ctx, &cff_header_out->offset_size));

    if (cff_header_out->major > 1) {
        return ERROR(CFF_ERR_UNSUPPORTED_VERSION);
    }

    return NULL;
}
