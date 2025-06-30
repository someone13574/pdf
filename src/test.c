#include "test.h"

#include <stdio.h>
#include <stdlib.h>

__attribute__((weak)) extern TestFunctionEntry __start_test_registry[];
__attribute__((weak)) extern TestFunctionEntry __stop_test_registry[];

int main(void) {
    TestFunctionEntry* start = __start_test_registry;
    TestFunctionEntry* stop = __stop_test_registry;

    printf("Registered functions:\n");
    for (TestFunctionEntry* entry = start; entry < stop; entry++) {
        struct TestResult result = entry->function();
        if (result.error_message) {
            printf(
                "Test `%s` failed at `%s:%d` with message `%s`\n",
                entry->name,
                result.error_file,
                result.error_line,
                result.error_message
            );

            free(result.error_message);
        } else {
            printf("Test `%s` succeeded\n", entry->name);
        }
    }

    return 0;
}
