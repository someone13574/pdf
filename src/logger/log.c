#include "logger/log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char* severity_text(
    LogSeverity severity,
    LogDiagVerbosity verbosity,
    LogDiagVerbosity group_verbosity
) {
    switch (severity) {
        case LOG_SEVERITY_DIAG: {
            LogDiagVerbosity effective_verbosity;
            if (verbosity >= group_verbosity) {
                effective_verbosity = verbosity - group_verbosity;
            } else {
                effective_verbosity = verbosity;
            }

            switch (effective_verbosity) {
                case LOG_DIAG_VERBOSITY_OFF:
                case LOG_DIAG_VERBOSITY_TRACE: {
                    return "\x1b[2m[DIAG]\x1b[0m ";
                }
                case LOG_DIAG_VERBOSITY_DEBUG: {
                    return "\x1b[34m[DIAG]\x1b[0m ";
                }
                case LOG_DIAG_VERBOSITY_INFO: {
                    return "\x1b[32m[DIAG]\x1b[0m ";
                }
            }
        }
        case LOG_SEVERITY_WARN: {
            return "\x1b[33m[WARN]\x1b[0m ";
        }
        case LOG_SEVERITY_ERROR: {
            return "\x1b[31m[ERROR]\x1b[0m";
        }
        case LOG_SEVERITY_PANIC: {
            return "\x1b[31m\x1b[1m[PANIC]\x1b[0m";
        }
    }
}

void logger_log(
    const char* group,
    LogSeverity severity,
    LogDiagVerbosity verbosity,
    LogDiagVerbosity group_verbosity,
    const char* file,
    unsigned long line,
    const char* fmt,
    ...
) {
    // Location
    int file_and_group_len = (int)strlen(file) + (int)strlen(group) + 6;
    if (line < 10) {
        file_and_group_len += 1;
    } else if (line < 100) {
        file_and_group_len += 2;
    } else if (line < 1000) {
        file_and_group_len += 3;
    } else if (line < 10000) {
        file_and_group_len += 4;
    }

    int pad = 0;
    int pad_available = 48;

    do {
        pad = pad_available - file_and_group_len;
        pad_available += 16;
    } while (pad < 0);

    printf("\x1b[4m%s:%lu\x1b[0m ", file, line);
    printf("\x1b[2m(%s)\x1b[0m%*s ", group, pad, "");

    // Severity
    printf("%s ", severity_text(severity, verbosity, group_verbosity));

    // Message
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    printf("\n");
}
