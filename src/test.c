#include "test.h"

#ifdef TEST

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "log.h"

#define TEST_ASSERT_INT_CMP_DEF(                                               \
    type,                                                                      \
    type_underscore,                                                           \
    cmp_type,                                                                  \
    cmp_op,                                                                    \
    print_ecp                                                                  \
)                                                                              \
    TestResult __test_assert_##cmp_type##_##type_underscore(                   \
        type a,                                                                \
        type b,                                                                \
        long double eps,                                                       \
        const char* file,                                                      \
        unsigned long line                                                     \
    ) {                                                                        \
        (void)eps;                                                             \
        if (a cmp_op b) {                                                      \
            return TEST_RESULT_PASS;                                           \
        }                                                                      \
        logger_log(                                                            \
            1,                                                                 \
            "test-assert",                                                     \
            LOG_LEVEL_ERROR,                                                   \
            file,                                                              \
            line,                                                              \
            "Assertion failed: " print_ecp " " #cmp_op " " print_ecp,          \
            a,                                                                 \
            b                                                                  \
        );                                                                     \
        return TEST_RESULT_FAIL;                                               \
    }

#define TEST_ASSERT_FLOAT_CMP_DEF(                                             \
    type,                                                                      \
    type_underscore,                                                           \
    cmp_type,                                                                  \
    cmp_op,                                                                    \
    join_op,                                                                   \
    cmp_op_dsp,                                                                \
    printf_cast,                                                               \
    print_ecp                                                                  \
)                                                                              \
    TestResult __test_assert_##cmp_type##_##type_underscore(                   \
        type a,                                                                \
        type b,                                                                \
        long double eps,                                                       \
        const char* file,                                                      \
        unsigned long line                                                     \
    ) {                                                                        \
        if (a - b cmp_op(type) eps join_op b - a cmp_op(type) eps) {           \
            return TEST_RESULT_PASS;                                           \
        }                                                                      \
        logger_log(                                                            \
            1,                                                                 \
            "test-assert",                                                     \
            LOG_LEVEL_ERROR,                                                   \
            file,                                                              \
            line,                                                              \
            "Assertion failed: " print_ecp " " #cmp_op_dsp " " print_ecp       \
            " (eps=%Lg)",                                                      \
            (printf_cast)a,                                                    \
            (printf_cast)b,                                                    \
            eps                                                                \
        );                                                                     \
        return TEST_RESULT_FAIL;                                               \
    }

TEST_ASSERT_INT_CMP_DEF(char, char, eq, ==, "'%c'")
TEST_ASSERT_INT_CMP_DEF(signed char, signed_char, eq, ==, "%hhd")
TEST_ASSERT_INT_CMP_DEF(unsigned char, unsigned_char, eq, ==, "%hhu")
TEST_ASSERT_INT_CMP_DEF(short, short, eq, ==, "%hd")
TEST_ASSERT_INT_CMP_DEF(unsigned short, unsigned_short, eq, ==, "%hu")
TEST_ASSERT_INT_CMP_DEF(int, int, eq, ==, "%d")
TEST_ASSERT_INT_CMP_DEF(unsigned int, unsigned_int, eq, ==, "%u")
TEST_ASSERT_INT_CMP_DEF(long, long, eq, ==, "%ld")
TEST_ASSERT_INT_CMP_DEF(unsigned long, unsigned_long, eq, ==, "%lu")
TEST_ASSERT_INT_CMP_DEF(long long, long_long, eq, ==, "%lld")
TEST_ASSERT_INT_CMP_DEF(unsigned long long, unsigned_long_long, eq, ==, "%llu")
TEST_ASSERT_INT_CMP_DEF(void*, void_ptr, eq, ==, "%p")
TEST_ASSERT_INT_CMP_DEF(bool, bool, eq, ==, "%d")
TEST_ASSERT_FLOAT_CMP_DEF(float, float, eq, <, &&, ==, double, "%g")
TEST_ASSERT_FLOAT_CMP_DEF(double, double, eq, <, &&, ==, double, "%g")
TEST_ASSERT_FLOAT_CMP_DEF(long double, long_double, eq, <, &&, ==, long double, "%Lg")

TEST_ASSERT_INT_CMP_DEF(char, char, ne, !=, "'%c'")
TEST_ASSERT_INT_CMP_DEF(signed char, signed_char, ne, !=, "%hhd")
TEST_ASSERT_INT_CMP_DEF(unsigned char, unsigned_char, ne, !=, "%hhu")
TEST_ASSERT_INT_CMP_DEF(short, short, ne, !=, "%hd")
TEST_ASSERT_INT_CMP_DEF(unsigned short, unsigned_short, ne, !=, "%hu")
TEST_ASSERT_INT_CMP_DEF(int, int, ne, !=, "%d")
TEST_ASSERT_INT_CMP_DEF(unsigned int, unsigned_int, ne, !=, "%u")
TEST_ASSERT_INT_CMP_DEF(long, long, ne, !=, "%ld")
TEST_ASSERT_INT_CMP_DEF(unsigned long, unsigned_long, ne, !=, "%lu")
TEST_ASSERT_INT_CMP_DEF(long long, long_long, ne, !=, "%lld")
TEST_ASSERT_INT_CMP_DEF(unsigned long long, unsigned_long_long, ne, !=, "%llu")
TEST_ASSERT_INT_CMP_DEF(void*, void_ptr, ne, !=, "%p")
TEST_ASSERT_INT_CMP_DEF(bool, bool, ne, !=, "%d")
TEST_ASSERT_FLOAT_CMP_DEF(float, float, ne, >, ||, !=, double, "%g")
TEST_ASSERT_FLOAT_CMP_DEF(double, double, ne, >, ||, !=, double, "%g")
TEST_ASSERT_FLOAT_CMP_DEF(
    long double,
    long_double,
    ne,
    >,
    ||,
    !=,
    long double,
    "%Lg"
)

TestResult __test_assert_eq_str(
    const char* a,
    const char* b,
    long double eps,
    const char* file,
    unsigned long line
) {
    (void)eps;
    if (!a || !b) {
        logger_log(
            1,
            "test-assert",
            LOG_LEVEL_ERROR,
            file,
            line,
            "String passed to test assertion is null"
        );
        return TEST_RESULT_FAIL;
    }
    if (strcmp(a, b) == 0) {
        return TEST_RESULT_PASS;
    }
    logger_log(
        1,
        "test-assert",
        LOG_LEVEL_ERROR,
        file,
        line,
        "Assertion failed: \"%s\" == \"%s\"",
        a,
        b
    );
    return TEST_RESULT_FAIL;
}

TestResult __test_assert_ne_str(
    const char* a,
    const char* b,
    long double eps,
    const char* file,
    unsigned long line
) {
    (void)eps;
    if (!a || !b) {
        logger_log(
            1,
            "test-assert",
            LOG_LEVEL_ERROR,
            file,
            line,
            "String passed to test assertion is null"
        );
        return TEST_RESULT_FAIL;
    }
    if (strcmp(a, b) != 0) {
        return TEST_RESULT_PASS;
    }
    logger_log(
        1,
        "test-assert",
        LOG_LEVEL_ERROR,
        file,
        line,
        "Assertion failed: \"%s\" != \"%s\"",
        a,
        b
    );
    return TEST_RESULT_FAIL;
}

__attribute__((weak)) extern _TestFuncEntry __start_test_registry[];
__attribute__((weak)) extern _TestFuncEntry __stop_test_registry[];

void print_line() {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) < 0 || w.ws_col == 0) {
        w.ws_col = 80; // fallback
    }

    for (int i = w.ws_col; i-- > 0;) {
        putchar('-');
    }

    putchar('\n');
}

#ifdef TEST_MAIN
int main(void) {
#else
int test_entry(void) {
#endif // TEST_MAIN
    _TestFuncEntry* start = __start_test_registry;
    _TestFuncEntry* stop = __stop_test_registry;

    ptrdiff_t num_tests = stop - start;
    LOG_INFO_G("test", "Running %ld tests...", num_tests);
    print_line();

    int passed = 0;
    int failed = 0;

    for (_TestFuncEntry* entry = start; entry < stop; entry++) {
#ifndef TEST_NO_CAPTURE
        int pipefd[2];
        if (pipe(pipefd) < 0) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
#endif // TEST_NO_CAPTURE

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (pid == 0) {
#ifndef TEST_NO_CAPTURE
            close(pipefd[0]); // we only write

            // Save stdout for restore
            int saved_stdout = dup(STDOUT_FILENO);
            if (saved_stdout < 0) {
                perror("dup");
                _exit(EXIT_FAILURE);
            }

            // Redirect stdout to pipe
            if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
                perror("dup2");
                _exit(EXIT_FAILURE);
            }
            close(pipefd[1]);
#endif // TEST_NO_CAPTURE

            // Run test and capture output
            TestResult result = entry->func();

#ifndef TEST_NO_CAPTURE
            fflush(stdout);

            // Restore stdout
            if (dup2(saved_stdout, STDOUT_FILENO) < 0) {
                perror("dup2 restore");
                _exit(EXIT_FAILURE);
            }
            close(saved_stdout);
#endif // TEST_NO_CAPTURE

            _exit((result == TEST_RESULT_PASS) ? EXIT_SUCCESS : EXIT_FAILURE);
        } else {
#ifndef TEST_NO_CAPTURE
            close(pipefd[1]); // we only read

            // Save stdout into buffer
            char* buffer = NULL;
            size_t buffer_size = 0;
            ssize_t bytes_read;
            char read_buf[4096];

            while ((bytes_read = read(pipefd[0], read_buf, sizeof(read_buf)))
                   > 0) {
                // Grow buffer
                char* old_buffer = buffer;
                buffer = realloc(buffer, buffer_size + (size_t)bytes_read + 1);
                if (!buffer) {
                    free(old_buffer);
                    perror("realloc");
                    exit(EXIT_FAILURE);
                }

                // Copy new bytes to buffer
                memcpy(buffer + buffer_size, read_buf, (size_t)bytes_read);
                buffer_size += (size_t)bytes_read;
                buffer[buffer_size] = '\0';
            }
            close(pipefd[0]);
#endif // TEST_NO_CAPTURE

            int status;
            waitpid(pid, &status, 0);

            if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS) {
                LOG_DEBUG_G(
                    "test",
                    "Test `%s` (\x1b[4m%s:%lu\x1b[0m) passed",
                    entry->name,
                    entry->file,
                    entry->line
                );

#ifdef TEST_NO_CAPTURE
                print_line();
#endif // TEST_NO_CAPTURE

                passed++;
            } else {
                print_line();

#ifndef TEST_NO_CAPTURE
                if (buffer_size > 0) {
                    if (fwrite(buffer, 1, buffer_size, stdout) != buffer_size) {
                        perror("fwrite");
                        exit(EXIT_FAILURE);
                    }
                }
#endif // TEST_NO_CAPTURE

                LOG_ERROR_G(
                    "test",
                    "Test `%s` (\x1b[4m%s:%lu\x1b[0m) failed",
                    entry->name,
                    entry->file,
                    entry->line
                );

                print_line();

                failed++;
            }

#ifndef TEST_NO_CAPTURE
            free(buffer);
#endif // TEST_NO_CAPTURE
        }
    }

    print_line();
    LOG_INFO_G(
        "test",
        "Test results: %d/%d passed, %d/%d failed",
        passed,
        passed + failed,
        failed,
        passed + failed
    );

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

#else // TEST

void translation_unit_not_empty(void) {}

#endif
