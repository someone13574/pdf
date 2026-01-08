#pragma once

#include <stdint.h>
#include <time.h>

#include "err/error.h"
#include "parse_ctx/ctx.h"

typedef int16_t SfntShortFrac;
typedef int32_t SfntFixed;
typedef int16_t SfntFWord;
typedef uint16_t SfntUFWord;
typedef int64_t SfntLongDateTime;

#define DARRAY_NAME SfntFWordArray
#define DARRAY_LOWERCASE_NAME sfnt_fword_array
#define DARRAY_TYPE SfntFWord
#include "arena/darray_decl.h"

Error* sfnt_read_fixed(ParseCtx* ctx, SfntFixed* out);
Error* sfnt_read_fword(ParseCtx* ctx, SfntFWord* out);
Error* sfnt_read_ufword(ParseCtx* ctx, SfntUFWord* out);
Error* sfnt_read_long_date_time(ParseCtx* ctx, SfntLongDateTime* out);

double sfnt_short_frac_to_double(SfntShortFrac x);
double sfnt_fixed_to_double(SfntFixed x);
time_t sfnt_long_date_time_to_time(SfntLongDateTime time);
