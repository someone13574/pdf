#pragma once

#include <stdlib.h>

typedef enum {
    LOG_LEVEL_TRACE = 0,
    LOG_LEVEL_DEBUG = 1,
    LOG_LEVEL_INFO = 2,
    LOG_LEVEL_WARN = 3,
    LOG_LEVEL_ERROR = 4
} LogLevel;

void logger_log(
    int check_level,
    const char* group,
    LogLevel level,
    const char* file,
    unsigned long line,
    const char* fmt,
    ...
) __attribute__((format(printf, 6, 7)));

#if defined(SOURCE_PATH_SIZE)
#define RELATIVE_FILE_PATH (&__FILE__[SOURCE_PATH_SIZE])
#else
#define RELATIVE_FILE_PATH __FILE__
#endif

#define LOG_TRACE(...)                                                         \
    logger_log(                                                                \
        1,                                                                     \
        "",                                                                    \
        LOG_LEVEL_TRACE,                                                       \
        RELATIVE_FILE_PATH,                                                    \
        __LINE__,                                                              \
        __VA_ARGS__                                                            \
    )
#define LOG_DEBUG(...)                                                         \
    logger_log(                                                                \
        1,                                                                     \
        "",                                                                    \
        LOG_LEVEL_DEBUG,                                                       \
        RELATIVE_FILE_PATH,                                                    \
        __LINE__,                                                              \
        __VA_ARGS__                                                            \
    )
#define LOG_INFO(...)                                                          \
    logger_log(1, "", LOG_LEVEL_INFO, RELATIVE_FILE_PATH, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)                                                          \
    logger_log(1, "", LOG_LEVEL_WARN, RELATIVE_FILE_PATH, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...)                                                         \
    logger_log(                                                                \
        1,                                                                     \
        "",                                                                    \
        LOG_LEVEL_ERROR,                                                       \
        RELATIVE_FILE_PATH,                                                    \
        __LINE__,                                                              \
        __VA_ARGS__                                                            \
    )

#define LOG_TRACE_G(group, ...)                                                \
    logger_log(                                                                \
        1,                                                                     \
        group,                                                                 \
        LOG_LEVEL_TRACE,                                                       \
        RELATIVE_FILE_PATH,                                                    \
        __LINE__,                                                              \
        __VA_ARGS__                                                            \
    )
#define LOG_DEBUG_G(group, ...)                                                \
    logger_log(                                                                \
        1,                                                                     \
        group,                                                                 \
        LOG_LEVEL_DEBUG,                                                       \
        RELATIVE_FILE_PATH,                                                    \
        __LINE__,                                                              \
        __VA_ARGS__                                                            \
    )
#define LOG_INFO_G(group, ...)                                                 \
    logger_log(                                                                \
        1,                                                                     \
        group,                                                                 \
        LOG_LEVEL_INFO,                                                        \
        RELATIVE_FILE_PATH,                                                    \
        __LINE__,                                                              \
        __VA_ARGS__                                                            \
    )
#define LOG_WARN_G(group, ...)                                                 \
    logger_log(                                                                \
        1,                                                                     \
        group,                                                                 \
        LOG_LEVEL_WARN,                                                        \
        RELATIVE_FILE_PATH,                                                    \
        __LINE__,                                                              \
        __VA_ARGS__                                                            \
    )
#define LOG_ERROR_G(group, ...)                                                \
    logger_log(                                                                \
        1,                                                                     \
        group,                                                                 \
        LOG_LEVEL_ERROR,                                                       \
        RELATIVE_FILE_PATH,                                                    \
        __LINE__,                                                              \
        __VA_ARGS__                                                            \
    )

#define RELEASE_ASSERT(cond, ...)                                              \
    do {                                                                       \
        if (!(cond)) {                                                         \
            LOG_PANIC("Assertion failed: RELEASE_ASSERT(" #cond                \
                      ")" __VA_ARGS__);                                        \
        }                                                                      \
    } while (0)

#define LOG_TODO(...) LOG_PANIC("TODO" __VA_OPT__(": ") __VA_ARGS__)

#define LOG_PANIC(...)                                                         \
    do {                                                                       \
        LOG_ERROR(__VA_ARGS__);                                                \
        exit(EXIT_FAILURE);                                                    \
    } while (0)

#ifdef DEBUG

#define DEBUG_ASSERT(cond)                                                     \
    do {                                                                       \
        if (!(cond)) {                                                         \
            LOG_ERROR("Assertion failed: DEBUG_ASSERT(" #cond ")");            \
            exit(EXIT_FAILURE);                                                \
        }                                                                      \
    } while (0)

#else

#define DEBUG_ASSERT(cond)                                                     \
    do {                                                                       \
    } while (0)

#endif
