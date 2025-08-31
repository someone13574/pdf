#include "sfnt/types.h"

double from_sfnt_short_frac(SfntShortFrac x) {
    return (double)x / (double)(1 << 14);
}

double from_sfnt_fixed(SfntFixed x) { return (double)x / (double)(1 << 16); }

static const int64_t SFNT_EPOCH_DELTA = 2082844800;

time_t from_sfnt_long_date_time(SfntLongDateTime time) {
    return (time_t)(time - SFNT_EPOCH_DELTA);
}
