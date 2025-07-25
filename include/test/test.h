#pragma once

#ifdef TEST

#include "logger/log.h"

#if defined(SOURCE_PATH_SIZE)
#define RELATIVE_FILE_PATH (&__FILE__[SOURCE_PATH_SIZE])
#else
#define RELATIVE_FILE_PATH __FILE__
#endif

int test_entry(void);

typedef enum { TEST_RESULT_PASS, TEST_RESULT_FAIL } TestResult;

#define __TEST_ASSERT_INT_CMP_DECL(type, type_underscore, cmp_type)            \
    TestResult __test_assert_##cmp_type##_##type_underscore(                   \
        type a,                                                                \
        type b,                                                                \
        long double eps,                                                       \
        const char* file,                                                      \
        unsigned long line,                                                    \
        const char* extra_message,                                             \
        ...                                                                    \
    );

#define __TEST_ASSERT_FLOAT_CMP_DECL(type, type_underscore, cmp_type)          \
    TestResult __test_assert_##cmp_type##_##type_underscore(                   \
        type a,                                                                \
        type b,                                                                \
        long double eps,                                                       \
        const char* file,                                                      \
        unsigned long line,                                                    \
        const char* extra_message,                                             \
        ...                                                                    \
    );

__TEST_ASSERT_INT_CMP_DECL(char, char, eq)
__TEST_ASSERT_INT_CMP_DECL(signed char, signed_char, eq)
__TEST_ASSERT_INT_CMP_DECL(unsigned char, unsigned_char, eq)
__TEST_ASSERT_INT_CMP_DECL(short, short, eq)
__TEST_ASSERT_INT_CMP_DECL(unsigned short, unsigned_short, eq)
__TEST_ASSERT_INT_CMP_DECL(int, int, eq)
__TEST_ASSERT_INT_CMP_DECL(unsigned int, unsigned_int, eq)
__TEST_ASSERT_INT_CMP_DECL(long, long, eq)
__TEST_ASSERT_INT_CMP_DECL(unsigned long, unsigned_long, eq)
__TEST_ASSERT_INT_CMP_DECL(long long, long_long, eq)
__TEST_ASSERT_INT_CMP_DECL(unsigned long long, unsigned_long_long, eq)
__TEST_ASSERT_INT_CMP_DECL(void*, void_ptr, eq)
__TEST_ASSERT_INT_CMP_DECL(bool, bool, eq)
__TEST_ASSERT_FLOAT_CMP_DECL(float, float, eq)
__TEST_ASSERT_FLOAT_CMP_DECL(double, double, eq)
__TEST_ASSERT_FLOAT_CMP_DECL(long double, long_double, eq)

__TEST_ASSERT_INT_CMP_DECL(char, char, ne)
__TEST_ASSERT_INT_CMP_DECL(signed char, signed_char, ne)
__TEST_ASSERT_INT_CMP_DECL(unsigned char, unsigned_char, ne)
__TEST_ASSERT_INT_CMP_DECL(short, short, ne)
__TEST_ASSERT_INT_CMP_DECL(unsigned short, unsigned_short, ne)
__TEST_ASSERT_INT_CMP_DECL(int, int, ne)
__TEST_ASSERT_INT_CMP_DECL(unsigned int, unsigned_int, ne)
__TEST_ASSERT_INT_CMP_DECL(long, long, ne)
__TEST_ASSERT_INT_CMP_DECL(unsigned long, unsigned_long, ne)
__TEST_ASSERT_INT_CMP_DECL(long long, long_long, ne)
__TEST_ASSERT_INT_CMP_DECL(unsigned long long, unsigned_long_long, ne)
__TEST_ASSERT_INT_CMP_DECL(void*, void_ptr, ne)
__TEST_ASSERT_INT_CMP_DECL(bool, bool, ne)
__TEST_ASSERT_FLOAT_CMP_DECL(float, float, ne)
__TEST_ASSERT_FLOAT_CMP_DECL(double, double, ne)
__TEST_ASSERT_FLOAT_CMP_DECL(long double, long_double, ne)

TestResult __test_assert_eq_str(
    const char* a,
    const char* b,
    long double eps,
    const char* file,
    unsigned long line,
    const char* extra_message,
    ...
);

TestResult __test_assert_ne_str(
    const char* a,
    const char* b,
    long double eps,
    const char* file,
    unsigned long line,
    const char* extra_message,
    ...
);

#define __TEST_ASSERT_CMP_FN(x, cmp)                                           \
    _Generic(                                                                  \
        (x),                                                                   \
        char: __test_assert_##cmp##_char,                                      \
        signed char: __test_assert_##cmp##_signed_char,                        \
        unsigned char: __test_assert_##cmp##_unsigned_char,                    \
        short: __test_assert_##cmp##_short,                                    \
        unsigned short: __test_assert_##cmp##_unsigned_short,                  \
        int: __test_assert_##cmp##_int,                                        \
        unsigned int: __test_assert_##cmp##_unsigned_int,                      \
        long: __test_assert_##cmp##_long,                                      \
        unsigned long: __test_assert_##cmp##_unsigned_long,                    \
        long long: __test_assert_##cmp##_long_long,                            \
        unsigned long long: __test_assert_##cmp##_unsigned_long_long,          \
        void*: __test_assert_##cmp##_void_ptr,                                 \
        bool: __test_assert_##cmp##_bool,                                      \
        float: __test_assert_##cmp##_float,                                    \
        double: __test_assert_##cmp##_double,                                  \
        long double: __test_assert_##cmp##_long_double,                        \
        char*: __test_assert_##cmp##_str,                                      \
        const char*: __test_assert_##cmp##_str                                 \
    )

#define __TEST_TYPE_IS_STR(x) _Generic(x, char*: 1, const char*: 1, default: 0)

#define __TEST_ASSERT_EXTRA_MESSAGE_HELPER(...) ""
#define __TEST_ASSERT_EXTRA_MESSAGE_HELPER1(...) __VA_ARGS__
#define __TEST_ASSERT_EXTRA_MESSAGE(cond, ...)                                 \
    __TEST_ASSERT_EXTRA_MESSAGE_HELPER##cond(__VA_ARGS__)

#define TEST_ASSERT_EQ_EPS(a, b, eps, ...)                                     \
    do {                                                                       \
        _Static_assert(                                                        \
            __builtin_types_compatible_p(long double, __typeof__(eps)),        \
            "Epsilon must be a long double"                                    \
        );                                                                     \
        __typeof__(a) _a = (a);                                                \
        __typeof__(b) _b = (b);                                                \
        __typeof__(eps) _eps = (eps);                                          \
        _Static_assert(                                                        \
            __builtin_types_compatible_p(__typeof__(_a), __typeof__(_b))       \
            || (__TEST_TYPE_IS_STR(_a) && __TEST_TYPE_IS_STR(_b))              \
        );                                                                     \
        if (__TEST_ASSERT_CMP_FN(_a, eq)(                                      \
                _a,                                                            \
                _b,                                                            \
                _eps,                                                          \
                RELATIVE_FILE_PATH,                                            \
                __LINE__,                                                      \
                __TEST_ASSERT_EXTRA_MESSAGE(__VA_OPT__(1), __VA_ARGS__)        \
            )                                                                  \
            == TEST_RESULT_FAIL) {                                             \
            return TEST_RESULT_FAIL;                                           \
        }                                                                      \
    } while (0)

#define TEST_ASSERT_NE_EPS(a, b, eps, ...)                                     \
    do {                                                                       \
        _Static_assert(                                                        \
            __builtin_types_compatible_p(long double, __typeof__(eps)),        \
            "Epsilon must be a long double"                                    \
        );                                                                     \
        __typeof__(a) _a = (a);                                                \
        __typeof__(b) _b = (b);                                                \
        __typeof__(eps) _eps = (eps);                                          \
        _Static_assert(                                                        \
            __builtin_types_compatible_p(__typeof__(_a), __typeof__(_b))       \
            || (__TEST_TYPE_IS_STR(_a) && __TEST_TYPE_IS_STR(_b))              \
        );                                                                     \
        if (__TEST_ASSERT_CMP_FN(_a, ne __VA_OPT__(, ) __VA_ARGS__)(           \
                _a,                                                            \
                _b,                                                            \
                _eps,                                                          \
                RELATIVE_FILE_PATH,                                            \
                __LINE__,                                                      \
                __TEST_ASSERT_EXTRA_MESSAGE(__VA_OPT__(1), __VA_ARGS__)        \
            )                                                                  \
            == TEST_RESULT_FAIL) {                                             \
            return TEST_RESULT_FAIL;                                           \
        }                                                                      \
    } while (0)

#define TEST_ASSERT_EQ(a, b, ...)                                              \
    TEST_ASSERT_EQ_EPS(a, b, 1e-6L __VA_OPT__(, ) __VA_ARGS__)
#define TEST_ASSERT_NE(a, b, ...)                                              \
    TEST_ASSERT_NE_EPS(a, b, 1e-6L __VA_OPT__(, ) __VA_ARGS__)

#define TEST_ASSERT(cond, ...)                                                 \
    do {                                                                       \
        if (!(cond)) {                                                         \
            LOG_ERROR(                                                         \
                TEST,                                                          \
                "Assertion failed: \"" #cond "\"" __VA_OPT__(": ") __VA_ARGS__ \
            );                                                                 \
            return TEST_RESULT_FAIL;                                           \
        }                                                                      \
    } while (0)

typedef struct {
    TestResult (*func)(void);
    const char* name;
    const char* file;
    unsigned long line;
} _TestFuncEntry;

#define TEST_FUNC(test_name)                                                   \
    static TestResult test_name(void);                                         \
    static const _TestFuncEntry _test_entry_##test_name                        \
        __attribute__((used, section("test_registry"))) = {                    \
            .func = (test_name),                                               \
            .name = #test_name,                                                \
            .file = RELATIVE_FILE_PATH,                                        \
            .line = __LINE__                                                   \
    };                                                                         \
    TestResult test_name(void)

#endif // TEST
