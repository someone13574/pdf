#include "test/test.h"

int main(void) {
#ifdef TEST
    return test_entry();
#else // TEST
    return 0;
#endif
}
