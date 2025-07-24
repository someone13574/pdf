#pragma once

#include <stdlib.h>

#include "log_groups.h"

typedef enum {
    LOG_SEVERITY_DIAG,
    LOG_SEVERITY_WARN,
    LOG_SEVERITY_ERROR,
    LOG_SEVERITY_PANIC
} LogSeverity;

typedef enum {
    LOG_DIAG_VERBOSITY_TRACE = 0,
    LOG_DIAG_VERBOSITY_DEBUG = 1,
    LOG_DIAG_VERBOSITY_INFO = 2,
    LOG_DIAG_VERBOSITY_OFF = 3
} LogDiagVerbosity;

#define LOG_EXPAND_TRACE(parent) LOG_DIAG_VERBOSITY_TRACE
#define LOG_EXPAND_DEBUG(parent) LOG_DIAG_VERBOSITY_DEBUG
#define LOG_EXPAND_INFO(parent) LOG_DIAG_VERBOSITY_INFO
#define LOG_EXPAND_OFF(parent) LOG_DIAG_VERBOSITY_OFF
#define LOG_EXPAND_INHERIT(parent) LOG_GROUP_##parent##_VERBOSITY

#define X(group, verbosity, parent)                                            \
    enum { LOG_GROUP_##group##_VERBOSITY = LOG_EXPAND_##verbosity(parent) };

LOG_GROUPS

#undef X

#undef LOG_EXPAND_TRACE
#undef LOG_EXPAND_DEBUG
#undef LOG_EXPAND_INFO
#undef LOG_EXPAND_INHERIT

void logger_log(
    const char* group,
    LogSeverity severity,
    LogDiagVerbosity verbosity,
    LogDiagVerbosity group_verbosity,
    const char* file,
    unsigned long line,
    const char* fmt,
    ...
) __attribute__((format(printf, 7, 8)));

#if defined(SOURCE_PATH_SIZE)
#define RELATIVE_FILE_PATH (&__FILE__[SOURCE_PATH_SIZE])
#else
#define RELATIVE_FILE_PATH __FILE__
#endif

#define LOG_DIAG(verbosity, group, ...)                                        \
    do {                                                                       \
        if (LOG_DIAG_VERBOSITY_##verbosity >= LOG_GROUP_##group##_VERBOSITY) { \
            logger_log(                                                        \
                #group,                                                        \
                LOG_SEVERITY_DIAG,                                             \
                LOG_DIAG_VERBOSITY_##verbosity,                                \
                LOG_GROUP_##group##_VERBOSITY,                                 \
                RELATIVE_FILE_PATH,                                            \
                __LINE__,                                                      \
                __VA_ARGS__                                                    \
            );                                                                 \
        }                                                                      \
    } while (0)

#define LOG_WARN(group, ...)                                                   \
    do {                                                                       \
        (void)LOG_GROUP_##group##_VERBOSITY;                                   \
        logger_log(                                                            \
            #group,                                                            \
            LOG_SEVERITY_WARN,                                                 \
            LOG_DIAG_VERBOSITY_INFO,                                           \
            LOG_DIAG_VERBOSITY_TRACE,                                          \
            RELATIVE_FILE_PATH,                                                \
            __LINE__,                                                          \
            __VA_ARGS__                                                        \
        );                                                                     \
    } while (0)

#define LOG_ERROR(group, ...)                                                  \
    do {                                                                       \
        (void)LOG_GROUP_##group##_VERBOSITY;                                   \
        logger_log(                                                            \
            #group,                                                            \
            LOG_SEVERITY_ERROR,                                                \
            LOG_DIAG_VERBOSITY_INFO,                                           \
            LOG_DIAG_VERBOSITY_TRACE,                                          \
            RELATIVE_FILE_PATH,                                                \
            __LINE__,                                                          \
            __VA_ARGS__                                                        \
        );                                                                     \
    } while (0)

#define LOG_PANIC(...)                                                         \
    do {                                                                       \
        logger_log(                                                            \
            "PANIC",                                                           \
            LOG_SEVERITY_PANIC,                                                \
            LOG_DIAG_VERBOSITY_INFO,                                           \
            LOG_DIAG_VERBOSITY_TRACE,                                          \
            RELATIVE_FILE_PATH,                                                \
            __LINE__,                                                          \
            __VA_ARGS__                                                        \
        );                                                                     \
        exit(EXIT_FAILURE);                                                    \
    } while (0)

#define LOG_TODO(...) LOG_PANIC("TODO" __VA_OPT__(": ") __VA_ARGS__)

#define RELEASE_ASSERT(cond, ...)                                              \
    do {                                                                       \
        if (!(cond)) {                                                         \
            LOG_PANIC("Assertion failed: RELEASE_ASSERT(" #cond                \
                      ")" __VA_ARGS__);                                        \
        }                                                                      \
    } while (0)

#ifdef DEBUG
#define DEBUG_ASSERT(cond)                                                     \
    do {                                                                       \
        if (!(cond)) {                                                         \
            LOG_PANIC("Assertion failed: DEBUG_ASSERT(" #cond ")");            \
        }                                                                      \
    } while (0)
#else
#define DEBUG_ASSERT(cond)                                                     \
    do {                                                                       \
    } while (0)
#endif
