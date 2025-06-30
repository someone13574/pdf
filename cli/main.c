#include "log.h"
#include "pdf.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    LOG_DEBUG("Hello world %d", add(5, 3));

    return 0;
}
