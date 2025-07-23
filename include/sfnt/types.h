#pragma once

#include <stdint.h>
#include <time.h>

typedef int16_t SfntShortFrac;
typedef int32_t SfntFixed;
typedef int16_t SfntFWord;
typedef uint16_t SfntUFWord;
typedef int64_t SfntLongDateTime;

double from_sfnt_short_frac(SfntShortFrac x);
double from_sfnt_fixed(SfntFixed x);
time_t from_sfnt_long_date_time(SfntLongDateTime time);
