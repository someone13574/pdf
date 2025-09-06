#include "test/test.h"

#ifdef TEST

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "logger/log.h"

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
        unsigned long line,                                                    \
        const char* extra_message,                                             \
        ...                                                                    \
    ) {                                                                        \
        (void)eps;                                                             \
        if (a cmp_op b) {                                                      \
            return TEST_RESULT_PASS;                                           \
        }                                                                      \
        if (extra_message && *extra_message) {                                 \
            va_list args;                                                      \
            va_start(args, extra_message);                                     \
            char extra_message_buf[256];                                       \
            vsnprintf(                                                         \
                extra_message_buf,                                             \
                sizeof(extra_message_buf),                                     \
                extra_message,                                                 \
                args                                                           \
            );                                                                 \
            va_end(args);                                                      \
            logger_log(                                                        \
                "TEST",                                                        \
                LOG_SEVERITY_PANIC,                                            \
                LOG_DIAG_VERBOSITY_INFO,                                       \
                LOG_DIAG_VERBOSITY_TRACE,                                      \
                file,                                                          \
                line,                                                          \
                "Assertion failed: (" print_ecp " " #cmp_op " " print_ecp      \
                "): %s",                                                       \
                a,                                                             \
                b,                                                             \
                extra_message_buf                                              \
            );                                                                 \
        } else {                                                               \
            logger_log(                                                        \
                "TEST",                                                        \
                LOG_SEVERITY_PANIC,                                            \
                LOG_DIAG_VERBOSITY_INFO,                                       \
                LOG_DIAG_VERBOSITY_TRACE,                                      \
                file,                                                          \
                line,                                                          \
                "Assertion failed: (" print_ecp " " #cmp_op " " print_ecp ")", \
                a,                                                             \
                b                                                              \
            );                                                                 \
        }                                                                      \
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
        unsigned long line,                                                    \
        const char* extra_message,                                             \
        ...                                                                    \
    ) {                                                                        \
        if (a - b cmp_op(type) eps join_op b - a cmp_op(type) eps) {           \
            return TEST_RESULT_PASS;                                           \
        }                                                                      \
        if (extra_message && *extra_message) {                                 \
            va_list args;                                                      \
            va_start(args, extra_message);                                     \
            char extra_message_buf[256];                                       \
            vsnprintf(                                                         \
                extra_message_buf,                                             \
                sizeof(extra_message_buf),                                     \
                extra_message,                                                 \
                args                                                           \
            );                                                                 \
            va_end(args);                                                      \
            logger_log(                                                        \
                "TEST",                                                        \
                LOG_SEVERITY_PANIC,                                            \
                LOG_DIAG_VERBOSITY_INFO,                                       \
                LOG_DIAG_VERBOSITY_TRACE,                                      \
                file,                                                          \
                line,                                                          \
                "Assertion failed: (" print_ecp " " #cmp_op_dsp " " print_ecp  \
                ") (eps=%Lg): %s",                                             \
                (printf_cast)a,                                                \
                (printf_cast)b,                                                \
                eps,                                                           \
                extra_message_buf                                              \
            );                                                                 \
        } else {                                                               \
            logger_log(                                                        \
                "TEST",                                                        \
                LOG_SEVERITY_PANIC,                                            \
                LOG_DIAG_VERBOSITY_INFO,                                       \
                LOG_DIAG_VERBOSITY_TRACE,                                      \
                file,                                                          \
                line,                                                          \
                "Assertion failed: (" print_ecp " " #cmp_op_dsp " " print_ecp  \
                ") (eps=%Lg)",                                                 \
                (printf_cast)a,                                                \
                (printf_cast)b,                                                \
                eps                                                            \
            );                                                                 \
        }                                                                      \
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
    unsigned long line,
    const char* extra_message,
    ...
) {
    (void)eps;
    if (!a || !b) {
        logger_log(
            "TEST",
            LOG_SEVERITY_PANIC,
            LOG_DIAG_VERBOSITY_INFO,
            LOG_DIAG_VERBOSITY_TRACE,
            file,
            line,
            "String passed to test assertion is null"
        );
        return TEST_RESULT_FAIL;
    }
    if (strcmp(a, b) == 0) {
        return TEST_RESULT_PASS;
    }
    if (extra_message && *extra_message) {
        va_list args;
        va_start(args, extra_message);
        char extra_message_buf[256];
        vsnprintf(
            extra_message_buf,
            sizeof(extra_message_buf),
            extra_message,
            args
        );
        va_end(args);
        logger_log(
            "TEST",
            LOG_SEVERITY_PANIC,
            LOG_DIAG_VERBOSITY_INFO,
            LOG_DIAG_VERBOSITY_TRACE,
            file,
            line,
            "Assertion failed: (\"%s\" == \"%s\"): %s",
            a,
            b,
            extra_message_buf
        );
    } else {
        logger_log(
            "TEST",
            LOG_SEVERITY_PANIC,
            LOG_DIAG_VERBOSITY_INFO,
            LOG_DIAG_VERBOSITY_TRACE,
            file,
            line,
            "Assertion failed: (\"%s\" == \"%s\")",
            a,
            b
        );
    }
    return TEST_RESULT_FAIL;
}

TestResult __test_assert_ne_str(
    const char* a,
    const char* b,
    long double eps,
    const char* file,
    unsigned long line,
    const char* extra_message,
    ...
) {
    (void)eps;
    if (!a || !b) {
        logger_log(
            "TEST",
            LOG_SEVERITY_PANIC,
            LOG_DIAG_VERBOSITY_INFO,
            LOG_DIAG_VERBOSITY_TRACE,
            file,
            line,
            "String passed to test assertion is null"
        );
        return TEST_RESULT_FAIL;
    }
    if (strcmp(a, b) != 0) {
        return TEST_RESULT_PASS;
    }
    if (extra_message && *extra_message) {
        va_list args;
        va_start(args, extra_message);
        char extra_message_buf[256];
        vsnprintf(
            extra_message_buf,
            sizeof(extra_message_buf),
            extra_message,
            args
        );
        va_end(args);
        logger_log(
            "TEST",
            LOG_SEVERITY_PANIC,
            LOG_DIAG_VERBOSITY_INFO,
            LOG_DIAG_VERBOSITY_TRACE,
            file,
            line,
            "Assertion failed: \"%s\" != \"%s\": %s",
            a,
            b,
            extra_message_buf
        );
    } else {
        logger_log(
            "TEST",
            LOG_SEVERITY_PANIC,
            LOG_DIAG_VERBOSITY_INFO,
            LOG_DIAG_VERBOSITY_TRACE,
            file,
            line,
            "Assertion failed: \"%s\" != \"%s\"",
            a,
            b
        );
    }
    return TEST_RESULT_FAIL;
}

#else // TEST

void translation_unit_not_empty(void) {}

#endif
