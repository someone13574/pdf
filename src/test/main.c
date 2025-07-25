#include "test/test.h"

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

#ifdef TEST
    return test_entry();
#else // TEST
    return 0;
#endif
}
