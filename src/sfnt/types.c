#include "sfnt/types.h"

#define DARRAY_NAME SfntUint8Array
#define DARRAY_LOWERCASE_NAME sfnt_uint8_array
#define DARRAY_TYPE uint8_t
#include "../arena/darray_impl.h"

#define DARRAY_NAME SfntInt16Array
#define DARRAY_LOWERCASE_NAME sfnt_int16_array
#define DARRAY_TYPE int16_t
#include "../arena/darray_impl.h"

#define DARRAY_NAME SfntUint16Array
#define DARRAY_LOWERCASE_NAME sfnt_uint16_array
#define DARRAY_TYPE uint16_t
#include "../arena/darray_impl.h"

#define DARRAY_NAME SfntUint32Array
#define DARRAY_LOWERCASE_NAME sfnt_uint32_array
#define DARRAY_TYPE uint32_t
#include "../arena/darray_impl.h"

double from_sfnt_short_frac(SfntShortFrac x) {
    return (double)x / (double)(1 << 14);
}

double from_sfnt_fixed(SfntFixed x) {
    return (double)x / (double)(1 << 16);
}

static const int64_t SFNT_EPOCH_DELTA = 2082844800;

time_t from_sfnt_long_date_time(SfntLongDateTime time) {
    return (time_t)(time - SFNT_EPOCH_DELTA);
}
