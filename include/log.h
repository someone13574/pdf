#pragma once

typedef enum {
    LOG_LEVEL_TRACE = 0,
    LOG_LEVEL_DEBUG = 1,
    LOG_LEVEL_INFO = 2,
    LOG_LEVEL_WARN = 3,
    LOG_LEVEL_ERROR = 4
} log_level_t;

void logger_log(int check_level, const char* group, log_level_t level, const char* file, unsigned long line, const char* fmt, ...);

#if defined (SOURCE_PATH_SIZE)
#define LOGGING_FILENAME (&__FILE__[SOURCE_PATH_SIZE])
#else
#define LOGGING_FILENAME __FILE__
#endif

#define LOG_TRACE(...) logger_log(1, "", LOG_LEVEL_TRACE, LOGGING_FILENAME, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(...) logger_log(1, "", LOG_LEVEL_DEBUG, LOGGING_FILENAME, __LINE__, __VA_ARGS__)
#define LOG_INFO(...) logger_log(1, "", LOG_LEVEL_INFO, LOGGING_FILENAME, __LINE__, __VA_ARGS__)
#define LOG_WARN(...) logger_log(1, "", LOG_LEVEL_WARN, LOGGING_FILENAME, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) logger_log(1, "", LOG_LEVEL_ERROR, LOGGING_FILENAME, __LINE__, __VA_ARGS__)

#define LOG_TRACE_G(group, ...) logger_log(1, group, LOG_LEVEL_TRACE, LOGGING_FILENAME, __LINE__, __VA_ARGS__)
#define LOG_DEBUG_G(group, ...) logger_log(1, group, LOG_LEVEL_DEBUG, LOGGING_FILENAME, __LINE__, __VA_ARGS__)
#define LOG_INFO_G(group, ...) logger_log(1, group, LOG_LEVEL_INFO, LOGGING_FILENAME, __LINE__, __VA_ARGS__)
#define LOG_WARN_G(group, ...) logger_log(1, group, LOG_LEVEL_WARN, LOGGING_FILENAME, __LINE__, __VA_ARGS__)
#define LOG_ERROR_G(group, ...) logger_log(1, group, LOG_LEVEL_ERROR, LOGGING_FILENAME, __LINE__, __VA_ARGS__)
