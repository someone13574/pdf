#pragma once

#include <stdint.h>
#include <time.h>

#define DARRAY_NAME SfntUint8Array
#define DARRAY_LOWERCASE_NAME sfnt_uint8_array
#define DARRAY_TYPE uint8_t
#include "arena/darray_decl.h"

#define DARRAY_NAME SfntInt16Array
#define DARRAY_LOWERCASE_NAME sfnt_int16_array
#define DARRAY_TYPE int16_t
#include "arena/darray_decl.h"

#define DARRAY_NAME SfntUint16Array
#define DARRAY_LOWERCASE_NAME sfnt_uint16_array
#define DARRAY_TYPE uint16_t
#include "arena/darray_decl.h"

#define DARRAY_NAME SfntUint32Array
#define DARRAY_LOWERCASE_NAME sfnt_uint32_array
#define DARRAY_TYPE uint32_t
#include "arena/darray_decl.h"

typedef int16_t SfntShortFrac;
typedef int32_t SfntFixed;
typedef int16_t SfntFWord;
typedef uint16_t SfntUFWord;
typedef int64_t SfntLongDateTime;

double from_sfnt_short_frac(SfntShortFrac x);
double from_sfnt_fixed(SfntFixed x);
time_t from_sfnt_long_date_time(SfntLongDateTime time);
