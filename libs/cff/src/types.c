#include "types.h"

double cff_num_as_real(CffNumber num) {
    if (num.type == CFF_NUMBER_INT) {
        return (double)num.value.integer;
    } else {
        return num.value.real;
    }
}
