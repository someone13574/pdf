#pragma once

typedef enum {
    LOG_LEVEL_TRACE = 0,
    LOG_LEVEL_DEBUG = 1,
    LOG_LEVEL_INFO = 2,
    LOG_LEVEL_WARN = 3,
    LOG_LEVEL_ERROR = 4
} LogLevel;

void logger_log(
    int check_level,
    char const* group,
    LogLevel level,
    char const* file,
    unsigned long line,
    char const* fmt,
    ...
);

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
