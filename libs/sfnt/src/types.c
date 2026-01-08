#include "sfnt/types.h"

#include "parse_ctx/ctx.h"

#define DARRAY_NAME SfntFWordArray
#define DARRAY_LOWERCASE_NAME sfnt_fword_array
#define DARRAY_TYPE SfntFWord
#include "arena/darray_impl.h"

Error* sfnt_read_fixed(ParseCtx* ctx, SfntFixed* out) {
    return parse_ctx_read_i32_be(ctx, out);
}

Error* sfnt_read_fword(ParseCtx* ctx, SfntFWord* out) {
    return parse_ctx_read_i16_be(ctx, out);
}

Error* sfnt_read_ufword(ParseCtx* ctx, SfntUFWord* out) {
    return parse_ctx_read_u16_be(ctx, out);
}

Error* sfnt_read_long_date_time(ParseCtx* ctx, SfntLongDateTime* out) {
    return parse_ctx_read_i64_be(ctx, out);
}

double sfnt_short_frac_to_double(SfntShortFrac x) {
    return (double)x / (double)(1 << 14);
}

double sfnt_fixed_to_double(SfntFixed x) {
    return (double)x / (double)(1 << 16);
}

static const int64_t SFNT_EPOCH_DELTA = 2082844800;

time_t sfnt_long_date_time_to_time(SfntLongDateTime time) {
    return (time_t)(time - SFNT_EPOCH_DELTA);
}
