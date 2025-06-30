#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(SOURCE_PATH_SIZE)
    #define RELATIVE_FILE_PATH (&__FILE__[SOURCE_PATH_SIZE])
#else
    #define RELATIVE_FILE_PATH __FILE__
#endif

typedef struct {
    char const* error_file;
    uint32_t error_line;
} TestResult;

typedef struct {
    TestResult (*function)(void);
    char const* name;
} __TestFunctionEntry;

static TestResult const TEST_SUCCESS = {.error_file = NULL, .error_line = 0};

#define TEST_PANIC(...)                                                        \
    do {                                                                       \
        LOG_ERROR_G("test", __VA_ARGS__);                                      \
        return (TestResult) {                                                  \
            .error_file = RELATIVE_FILE_PATH,                                  \
            .error_line = __LINE__,                                            \
        };                                                                     \
    } while (0)

#define TEST_ASSERT(condition, ...)                                            \
    do {                                                                       \
        if (!(condition)) {                                                    \
            LOG_ERROR_G("test", "Assertion failed: " __VA_ARGS__);             \
            return (TestResult) {                                              \
                .error_file = RELATIVE_FILE_PATH,                              \
                .error_line = __LINE__,                                        \
            };                                                                 \
        }                                                                      \
    } while (0)

// This should never be defined, so if the _Generic in __TEST_COERCE fails it
// will just lead to a linker error.
void* __test_undefined(void);

#define __TEST_COERCE(x, type)                                                 \
    _Generic(                                                                  \
        (x),                                                                   \
        type: (x), /* NOLINT(bugprone-macro-parentheses) */                    \
        default: *(type*)__test_undefined()                                    \
    )

#define __TEST_CMP_MESSAGE(format_str, a, b, msg_op)                           \
    "Assertion failed: (" #a ") " msg_op " (" #b "), (expected " format_str    \
    ", got " format_str ")"

#define __TEST_CMP_FLOAT(a, b, type, eps)                                      \
    __TEST_COERCE(a, type) - __TEST_COERCE(b, type) < (type)(eps)              \
        && __TEST_COERCE(b, type) - __TEST_COERCE(a, type) < (type)(eps)

#define TEST_ASSERT_CMP(a, b, target_eq, eps, msg_op)                          \
    do {                                                                       \
        __typeof__(eps) _eps = eps;                                            \
        _Static_assert(                                                        \
            __builtin_types_compatible_p(long double, __typeof__(_eps)),       \
            "Epsilon must be a long double"                                    \
        );                                                                     \
        __typeof__(a) _a = (a);                                                \
        __typeof__(b) _b = (b);                                                \
        _Static_assert(                                                        \
            __builtin_types_compatible_p(__typeof__(_a), __typeof__(_b))       \
                || (_Generic(_a, char*: 1, const char*: 1, default: 0)         \
                    && _Generic(_b, char*: 1, const char*: 1, default: 0)),    \
            "types of `" #a "` and `" #b "` do not match"                      \
        );                                                                     \
        _Bool equal = _Generic(                                                \
            _a,                                                                \
            float: __TEST_CMP_FLOAT(_a, _b, float, _eps),                      \
            double: __TEST_CMP_FLOAT(_a, _b, double, _eps),                    \
            long double: __TEST_CMP_FLOAT(_a, _b, long double, _eps),          \
            char*: strcmp(                                                     \
                __TEST_COERCE(_a, const char*),                                \
                __TEST_COERCE(_b, const char*)                                 \
            ) == 0,                                                            \
            const char*: strcmp(                                               \
                __TEST_COERCE(_a, const char*),                                \
                __TEST_COERCE(_b, const char*)                                 \
            ) == 0,                                                            \
            default: _a == _b                                                  \
        );                                                                     \
        if (((target_eq) && !equal) || (!(target_eq) && equal)) {              \
            LOG_ERROR_G(                                                       \
                "test",                                                        \
                _Generic(                                                      \
                    _a,                                                        \
                    char: __TEST_CMP_MESSAGE("'%c'", a, b, msg_op),            \
                    signed char: __TEST_CMP_MESSAGE("%hhd", a, b, msg_op),     \
                    unsigned char: __TEST_CMP_MESSAGE("%hhu", a, b, msg_op),   \
                    short: __TEST_CMP_MESSAGE("%hd", a, b, msg_op),            \
                    unsigned short: __TEST_CMP_MESSAGE("%hu", a, b, msg_op),   \
                    int: __TEST_CMP_MESSAGE("%d", a, b, msg_op),               \
                    unsigned int: __TEST_CMP_MESSAGE("%u", a, b, msg_op),      \
                    long: __TEST_CMP_MESSAGE("%ld", a, b, msg_op),             \
                    unsigned long: __TEST_CMP_MESSAGE("%lu", a, b, msg_op),    \
                    long long: __TEST_CMP_MESSAGE("%lld", a, b, msg_op),       \
                    unsigned long long: __TEST_CMP_MESSAGE(                    \
                        "%llu",                                                \
                        a,                                                     \
                        b,                                                     \
                        msg_op                                                 \
                    ),                                                         \
                    float: __TEST_CMP_MESSAGE(                                 \
                        "%f",                                                  \
                        a,                                                     \
                        b,                                                     \
                        msg_op                                                 \
                    ) " (eps=%Lg)",                                            \
                    double: __TEST_CMP_MESSAGE(                                \
                        "%f",                                                  \
                        a,                                                     \
                        b,                                                     \
                        msg_op                                                 \
                    ) " (eps=%Lg)",                                            \
                    long double: __TEST_CMP_MESSAGE(                           \
                        "%Lf",                                                 \
                        a,                                                     \
                        b,                                                     \
                        msg_op                                                 \
                    ) " (eps=%Lg)",                                            \
                    char*: __TEST_CMP_MESSAGE("\"%s\"", a, b, msg_op),         \
                    const char*: __TEST_CMP_MESSAGE("\"%s\"", a, b, msg_op),   \
                    void*: __TEST_CMP_MESSAGE("%p", a, b, msg_op),             \
                    default: "Assertion failed: (" #a ") " msg_op " (" #b ")"  \
                ),                                                             \
                _a,                                                            \
                _b,                                                            \
                _eps                                                           \
            );                                                                 \
            return (TestResult) {                                              \
                .error_file = RELATIVE_FILE_PATH,                              \
                .error_line = __LINE__,                                        \
            };                                                                 \
        }                                                                      \
    } while (0)

#define TEST_ASSERT_EQ(a, b) TEST_ASSERT_CMP(a, b, 1, 1e-6L, "==")
#define TEST_ASSERT_NE(a, b) TEST_ASSERT_CMP(a, b, 0, 1e-6L, "!=")
#define TEST_ASSERT_EQ_EPS(a, b, eps) TEST_ASSERT_CMP(a, b, 1, eps, "==")
#define TEST_ASSERT_NE_EPS(a, b, eps) TEST_ASSERT_CMP(a, b, 0, eps, "!=")

#define TEST_FUNC(test_name)                                                   \
    static TestResult test_name(void);                                         \
    static const __TestFunctionEntry _test_reg_##test_name                     \
        __attribute__((used, section("test_registry"))                         \
        ) = {.function = (test_name), .name = #test_name};                     \
    TestResult test_name(void)
