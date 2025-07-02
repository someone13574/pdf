#pragma once

#ifdef TEST

#include <log.h>
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
    const char* error_file;
    uint32_t error_line;
} TestResult;

typedef struct {
    TestResult (*function)(void);
    const char* name;
    const char* file;
    uint32_t line;
} __TestFunctionEntry;

static const TestResult TEST_SUCCESS = {.error_file = NULL, .error_line = 0};

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
            LOG_ERROR_G(                                                       \
                "test",                                                        \
                "Assertion failed: (" #condition "), " __VA_ARGS__             \
            );                                                                 \
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

#define __TEST_COERCE_STR(x)                                                   \
    _Generic(                                                                  \
        (x) + 0,                                                               \
        char*: (x),                                                            \
        const char*: (x),                                                      \
        default: *(const char**)__test_undefined()                             \
    )

#define __TEST_CMP_MESSAGE_CASE(                                               \
    format_str,                                                                \
    a,                                                                         \
    b,                                                                         \
    _a,                                                                        \
    _b,                                                                        \
    msg_op,                                                                    \
    expected,                                                                  \
    type                                                                       \
)                                                                              \
    type:                                                                      \
    LOG_ERROR_G(                                                               \
        "test",                                                                \
        "Assertion failed: (" #a ") " msg_op " (" #b                           \
        "), (expected " expected format_str ", got " format_str ")",           \
        __TEST_COERCE(_a, type),                                               \
        __TEST_COERCE(_b, type)                                                \
    )

#define __TEST_CMP_MESSAGE_CASE_STR(                                           \
    format_str,                                                                \
    a,                                                                         \
    b,                                                                         \
    _a,                                                                        \
    _b,                                                                        \
    msg_op,                                                                    \
    expected,                                                                  \
    type                                                                       \
)                                                                              \
    type:                                                                      \
    LOG_ERROR_G(                                                               \
        "test",                                                                \
        "Assertion failed: (" #a ") " msg_op " (" #b                           \
        "), (expected " expected format_str ", got " format_str ")",           \
        __TEST_COERCE_STR(_a),                                                 \
        __TEST_COERCE_STR(_b)                                                  \
    )

#define __TEST_CMP_MESSAGE_CASE_EPS(                                           \
    format_str,                                                                \
    a,                                                                         \
    b,                                                                         \
    _a,                                                                        \
    _b,                                                                        \
    eps,                                                                       \
    msg_op,                                                                    \
    expected,                                                                  \
    type                                                                       \
)                                                                              \
    type:                                                                      \
    LOG_ERROR_G(                                                               \
        "test",                                                                \
        "Assertion failed: (" #a ") " msg_op " (" #b                           \
        "), (expected " expected format_str ", got " format_str ") (eps=%Lg)", \
        __TEST_COERCE(_a, type),                                               \
        __TEST_COERCE(_b, type),                                               \
        eps                                                                    \
    )

#define __TEST_CMP_FLOAT(a, b, type, eps)                                      \
    __TEST_COERCE(a, type) - __TEST_COERCE(b, type) < (type)(eps)              \
        && __TEST_COERCE(b, type) - __TEST_COERCE(a, type) < (type)(eps)

#define TEST_ASSERT_CMP(a, b, target_eq, eps, msg_op, expected_msg)                                     \
    do {                                                                                                \
        __typeof__(eps) _eps = eps;                                                                     \
        _Static_assert(                                                                                 \
            __builtin_types_compatible_p(long double, __typeof__(_eps)),                                \
            "Epsilon must be a long double"                                                             \
        );                                                                                              \
        __typeof__(a) _a = (a);                                                                         \
        __typeof__(b) _b = (b);                                                                         \
        _Static_assert(                                                                                 \
            __builtin_types_compatible_p(__typeof__(_a), __typeof__(_b))                                \
                || (_Generic(_a, char*: 1, const char*: 1, default: 0)                                  \
                    && _Generic(_b, char*: 1, const char*: 1, default: 0)),                             \
            "types of `" #a "` and `" #b "` do not match"                                               \
        );                                                                                              \
        _Bool equal = _Generic(                                                                         \
            _a,                                                                                         \
            float: __TEST_CMP_FLOAT(_a, _b, float, _eps),                                               \
            double: __TEST_CMP_FLOAT(_a, _b, double, _eps),                                             \
            long double: __TEST_CMP_FLOAT(_a, _b, long double, _eps),                                   \
            char*: strcmp(__TEST_COERCE_STR(_a), __TEST_COERCE_STR(_b)) == 0,                           \
            const char*: strcmp(__TEST_COERCE_STR(_a), __TEST_COERCE_STR(_b))                           \
                == 0,                                                                                   \
            default: _a == _b                                                                           \
        );                                                                                              \
        if (((target_eq) && !equal) || (!(target_eq) && equal)) {                                       \
            _Generic(                                                                                   \
                _a,                                                                                     \
                __TEST_CMP_MESSAGE_CASE(                                                                \
                    "'%c'",                                                                             \
                    a,                                                                                  \
                    b,                                                                                  \
                    _a,                                                                                 \
                    _b,                                                                                 \
                    msg_op,                                                                             \
                    expected_msg,                                                                       \
                    char                                                                                \
                ),                                                                                      \
                __TEST_CMP_MESSAGE_CASE(                                                                \
                    "%hhd",                                                                             \
                    a,                                                                                  \
                    b,                                                                                  \
                    _a,                                                                                 \
                    _b,                                                                                 \
                    msg_op,                                                                             \
                    expected_msg,                                                                       \
                    signed char                                                                         \
                ),                                                                                      \
                __TEST_CMP_MESSAGE_CASE(                                                                \
                    "%hhu",                                                                             \
                    a,                                                                                  \
                    b,                                                                                  \
                    _a,                                                                                 \
                    _b,                                                                                 \
                    msg_op,                                                                             \
                    expected_msg,                                                                       \
                    unsigned char                                                                       \
                ),                                                                                      \
                __TEST_CMP_MESSAGE_CASE(                                                                \
                    "%hd",                                                                              \
                    a,                                                                                  \
                    b,                                                                                  \
                    _a,                                                                                 \
                    _b,                                                                                 \
                    msg_op,                                                                             \
                    expected_msg,                                                                       \
                    short                                                                               \
                ),                                                                                      \
                __TEST_CMP_MESSAGE_CASE(                                                                \
                    "%hu",                                                                              \
                    a,                                                                                  \
                    b,                                                                                  \
                    _a,                                                                                 \
                    _b,                                                                                 \
                    msg_op,                                                                             \
                    expected_msg,                                                                       \
                    unsigned short                                                                      \
                ),                                                                                      \
                __TEST_CMP_MESSAGE_CASE(                                                                \
                    "%d",                                                                               \
                    a,                                                                                  \
                    b,                                                                                  \
                    _a,                                                                                 \
                    _b,                                                                                 \
                    msg_op,                                                                             \
                    expected_msg,                                                                       \
                    int                                                                                 \
                ),                                                                                      \
                __TEST_CMP_MESSAGE_CASE(                                                                \
                    "%u",                                                                               \
                    a,                                                                                  \
                    b,                                                                                  \
                    _a,                                                                                 \
                    _b,                                                                                 \
                    msg_op,                                                                             \
                    expected_msg,                                                                       \
                    unsigned int                                                                        \
                ),                                                                                      \
                __TEST_CMP_MESSAGE_CASE(                                                                \
                    "%ld",                                                                              \
                    a,                                                                                  \
                    b,                                                                                  \
                    _a,                                                                                 \
                    _b,                                                                                 \
                    msg_op,                                                                             \
                    expected_msg,                                                                       \
                    long                                                                                \
                ),                                                                                      \
                __TEST_CMP_MESSAGE_CASE(                                                                \
                    "%lu",                                                                              \
                    a,                                                                                  \
                    b,                                                                                  \
                    _a,                                                                                 \
                    _b,                                                                                 \
                    msg_op,                                                                             \
                    expected_msg,                                                                       \
                    unsigned long                                                                       \
                ),                                                                                      \
                __TEST_CMP_MESSAGE_CASE(                                                                \
                    "%lld",                                                                             \
                    a,                                                                                  \
                    b,                                                                                  \
                    _a,                                                                                 \
                    _b,                                                                                 \
                    msg_op,                                                                             \
                    expected_msg,                                                                       \
                    long long                                                                           \
                ),                                                                                      \
                __TEST_CMP_MESSAGE_CASE(                                                                \
                    "%llu",                                                                             \
                    a,                                                                                  \
                    b,                                                                                  \
                    _a,                                                                                 \
                    _b,                                                                                 \
                    msg_op,                                                                             \
                    expected_msg,                                                                       \
                    unsigned long long                                                                  \
                ),                                                                                      \
                __TEST_CMP_MESSAGE_CASE_EPS(                                                            \
                    "%f",                                                                               \
                    a,                                                                                  \
                    b,                                                                                  \
                    _a,                                                                                 \
                    _b,                                                                                 \
                    eps,                                                                                \
                    msg_op,                                                                             \
                    expected_msg,                                                                       \
                    float                                                                               \
                ),                                                                                      \
                __TEST_CMP_MESSAGE_CASE_EPS(                                                            \
                    "%f",                                                                               \
                    a,                                                                                  \
                    b,                                                                                  \
                    _a,                                                                                 \
                    _b,                                                                                 \
                    eps,                                                                                \
                    msg_op,                                                                             \
                    expected_msg,                                                                       \
                    double                                                                              \
                ),                                                                                      \
                __TEST_CMP_MESSAGE_CASE_EPS(                                                            \
                    "%Lf",                                                                              \
                    a,                                                                                  \
                    b,                                                                                  \
                    _a,                                                                                 \
                    _b,                                                                                 \
                    eps,                                                                                \
                    msg_op,                                                                             \
                    expected_msg,                                                                       \
                    long double                                                                         \
                ),                                                                                      \
                __TEST_CMP_MESSAGE_CASE_STR("\"%s\"", a, b, _a, _b, msg_op, expected_msg, char*),       \
                __TEST_CMP_MESSAGE_CASE_STR("\"%s\"", a, b, _a, _b, msg_op, expected_msg, const char*), \
                __TEST_CMP_MESSAGE_CASE("%p", a, b, _a, _b, msg_op, expected_msg, void*),               \
                default: LOG_ERROR_G(                                                                   \
                    "test",                                                                             \
                    "Assertion failed: (" #a ") " msg_op " (" #b ")"                                    \
                )                                                                                       \
            );                                                                                          \
            return (TestResult) {                                                                       \
                .error_file = RELATIVE_FILE_PATH,                                                       \
                .error_line = __LINE__,                                                                 \
            };                                                                                          \
        }                                                                                               \
    } while (0)

#define TEST_ASSERT_EQ(a, b) TEST_ASSERT_CMP(a, b, 1, 1e-6L, "==", "")
#define TEST_ASSERT_NE(a, b) TEST_ASSERT_CMP(a, b, 0, 1e-6L, "!=", "not ")
#define TEST_ASSERT_EQ_EPS(a, b, eps)                                          \
    TEST_ASSERT_CMP(a, b, 1, eps, "==", "not ")
#define TEST_ASSERT_NE_EPS(a, b, eps)                                          \
    TEST_ASSERT_CMP(a, b, 0, eps, "!=", "not ")

#define TEST_FUNC(test_name)                                                   \
    static TestResult _test_func_##test_name(void);                            \
    static const __TestFunctionEntry _test_reg_##test_name                     \
        __attribute__((used, section("test_registry"))) = {                    \
            .function = (_test_func_##test_name),                              \
            .name = #test_name,                                                \
            .file = RELATIVE_FILE_PATH,                                        \
            .line = __LINE__                                                   \
    };                                                                         \
    TestResult _test_func_##test_name(void)

#endif // TEST
